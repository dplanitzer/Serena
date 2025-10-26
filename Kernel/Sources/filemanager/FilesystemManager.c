//
//  FilesystemManager.c
//  kernel
//
//  Created by Dietmar Planitzer on 12/12/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "FilesystemManager.h"
#include <dispatchqueue/DispatchQueue.h>
#include <driver/disk/DiskDriver.h>
#include <filesystem/DiskContainer.h>
#include <filesystem/IOChannel.h>
#include <filesystem/kernfs/KernFS.h>
#include <filesystem/serenafs/SerenaFS.h>
#include <klib/List.h>
#include <kern/kalloc.h>
#include <kern/timespec.h>
#include <sched/mtx.h>
#include <Catalog.h>


typedef struct fsentry {
    ListNode                node;
    FilesystemRef _Nonnull  fs;
    InodeRef _Nonnull       driverNode;
} fsentry_t;

static void fsentry_destroy(fsentry_t* _Nullable self)
{
    if (self) {
        Object_Release(self->fs);
        self->fs = NULL;

        if (self->driverNode) {
            Inode_Relinquish(self->driverNode);
            self->driverNode = NULL;
        }

        kfree(self);    
    }
}

static errno_t fsentry_create(FilesystemRef _Consuming _Nonnull fs, InodeRef _Nonnull driverNode, fsentry_t* _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    fsentry_t* self;

    try(kalloc_cleared(sizeof(fsentry_t), (void**)&self));
    self->fs = fs;
    self->driverNode = Inode_Reacquire(driverNode);

    *pOutSelf = self;
    return EOK;

catch:
    fsentry_destroy(self);
    *pOutSelf = NULL;
    return err;
}



// A filesystem is initially on the 'filesystems' list and this list owns the FS.
// Only filesystems on the filesystems list are synced to disk.
// If a filesystem is forced unmounted then its FS entry is moved over to the
// reaper queue.
typedef struct FilesystemManager {
    DispatchQueueRef _Nonnull   dispatchQueue;
    mtx_t                       mtx;
    List                        filesystems;    // List<FSEntry>
    List                        reaperQueue;    // List<FSEntry>
} FilesystemManager;


static void _FilesystemManager_ScheduleAutoSync(FilesystemManagerRef _Nonnull self);



FilesystemManagerRef _Nonnull gFilesystemManager;

errno_t FilesystemManager_Create(FilesystemManagerRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    FilesystemManagerRef self;

    try(kalloc_cleared(sizeof(FilesystemManager), (void**)&self));
    mtx_init(&self->mtx);

catch:
    *pOutSelf = self;
    return err;
}

errno_t FilesystemManager_Start(FilesystemManagerRef _Nonnull self)
{
    decl_try_err();

    err = DispatchQueue_Create(0, 1, SCHED_QOS_URGENT, QOS_PRI_LOWEST, (DispatchQueueRef*)&self->dispatchQueue);
    if (err == EOK) {
        _FilesystemManager_ScheduleAutoSync(self);
    }

    return err;
}


FilesystemRef _Nonnull FilesystemManager_GetCatalog(FilesystemManagerRef _Nonnull self)
{
    return Catalog_GetFilesystem(gFSCatalog);
}

errno_t FilesystemManager_EstablishFilesystem(FilesystemManagerRef _Nonnull self, InodeRef _Locked _Nonnull driverNode, unsigned int mode, FilesystemRef _Nullable * _Nonnull pOutFs)
{
    decl_try_err();
    FSContainerRef fsContainer = NULL;
    FilesystemRef fs = NULL;
    IOChannelRef chan = NULL;
    fsentry_t* entry = NULL;

    try(Inode_CreateChannel(driverNode, mode, &chan));
    try(DiskContainer_Create(chan, &fsContainer));
    try(SerenaFS_Create(fsContainer, (SerenaFSRef*)&fs));
    try(fsentry_create(fs, driverNode, &entry));
    Object_Release(fsContainer);
    IOChannel_Release(chan);

    mtx_lock(&self->mtx);
    List_InsertAfterLast(&self->filesystems, &entry->node);
    mtx_unlock(&self->mtx);
    *pOutFs = fs;
    return EOK;

catch:
    Object_Release(fs);
    Object_Release(fsContainer);
    IOChannel_Release(chan);
    *pOutFs = NULL;
    return err;
}

errno_t FilesystemManager_StartFilesystem(FilesystemManagerRef _Nonnull self, FilesystemRef _Nonnull fs, const char* _Nonnull params)
{
    decl_try_err();

    if (instanceof(fs, KernFS)) {
        return EOK;
    }

    err = Filesystem_Start(fs, params);
    if (err == EOK) {
        err = Filesystem_Publish(fs);
        if (err != EOK) {
            Filesystem_Stop(fs, false);
        }
    }

    return err;
}

errno_t FilesystemManager_StopFilesystem(FilesystemManagerRef _Nonnull self, FilesystemRef _Nonnull fs, bool forced)
{
    decl_try_err();

    if (instanceof(fs, KernFS)) {
        return EOK;
    }

    err = Filesystem_Stop(fs, forced);
    if (err != EBUSY || (err == EBUSY && forced)) {
        Filesystem_Unpublish(fs);
    }
}

static fsentry_t* _Nullable _fsentry_for_fsid(FilesystemManagerRef _Locked _Nonnull self, fsid_t fsid)
{
    List_ForEach(&self->filesystems, fsentry_t,
        if (Filesystem_GetId(pCurNode->fs) == fsid) {
            return pCurNode;
        }
    );

    return NULL;
}

void FilesystemManager_DisbandFilesystem(FilesystemManagerRef _Nonnull self, FilesystemRef _Nonnull fs)
{
    if (instanceof(fs, KernFS)) {
        return;
    }

    Filesystem_Disconnect(fs);

    if (Filesystem_CanDestroy(fs)) {
        // Destroy the FS now
        mtx_lock(&self->mtx);
        fsentry_t* ep = _fsentry_for_fsid(self, Filesystem_GetId(fs));

        List_Remove(&self->filesystems, &ep->node);
        mtx_unlock(&self->mtx);
        fsentry_destroy(ep);
    }
    else {
        // Hand the FS over to our reaper queue
        mtx_lock(&self->mtx);
        fsentry_t* ep = _fsentry_for_fsid(self, Filesystem_GetId(fs));

        List_Remove(&self->filesystems, &ep->node);
        List_InsertAfterLast(&self->reaperQueue, &ep->node);
        mtx_unlock(&self->mtx);
    }
}

errno_t FilesystemManager_AcquireDriverNodeForFsid(FilesystemManagerRef _Nonnull self, fsid_t fsid, InodeRef _Nullable * _Nonnull pOutNode)
{
    decl_try_err();

    mtx_lock(&self->mtx);
    fsentry_t* ep = _fsentry_for_fsid(self, fsid);

    if (ep) {
        *pOutNode = Inode_Reacquire(ep->driverNode);
        err = EOK;
    }
    else {
        *pOutNode = NULL;
        err = ENODEV;
    }
    mtx_unlock(&self->mtx);
    
    return err;
}

void FilesystemManager_Sync(FilesystemManagerRef _Nonnull self)
{
    mtx_lock(&self->mtx);
    // XXX change this to run the syncs outside of the lock
    List_ForEach(&self->filesystems, fsentry_t,
        Filesystem_Sync(pCurNode->fs);
    );
    mtx_unlock(&self->mtx);
}

static void _FilesystemManager_ReapFilesystem(FilesystemManagerRef _Nonnull self, List* _Nonnull queue, fsentry_t* _Nonnull ep)
{
    List_Remove(queue, &ep->node);
    fsentry_destroy(ep);
}

// Tries to stop and destroy filesystems that are on the reaper queue.
static void _FilesystemManager_Reaper(FilesystemManagerRef _Nonnull self)
{
    List queue;

    // Take a snapshot of the reaper queue so that we can do our work without
    // having to hold the lock.
    mtx_lock(&self->mtx);
    queue = self->reaperQueue;
    self->reaperQueue = LIST_INIT;
    mtx_unlock(&self->mtx);

    
    // Kill as many filesystems as we can
    if (!List_IsEmpty(&queue)) {

        List_ForEach(&queue, fsentry_t,
            if (Filesystem_CanDestroy(pCurNode->fs)) {
                _FilesystemManager_ReapFilesystem(self, &queue, pCurNode);
            }
        );


        // Put the rest back on the reaper queue
        mtx_lock(&self->mtx);
        List_ForEach(&queue, fsentry_t,
            List_Remove(&queue, &pCurNode->node);
            List_InsertBeforeFirst(&self->reaperQueue, &pCurNode->node);
        );
        mtx_unlock(&self->mtx);
    }
}

static void _FilesystemManager_DoBgWork(FilesystemManagerRef _Nonnull self)
{
    FilesystemManager_Sync(self);
    _FilesystemManager_Reaper(self);
}

// Schedule an automatic sync of cached blocks to the disk(s)
static void _FilesystemManager_ScheduleAutoSync(FilesystemManagerRef _Nonnull self)
{
    struct timespec interval;

    timespec_from_sec(&interval, 30);
    try_bang(DispatchQueue_DispatchAsyncPeriodically(self->dispatchQueue, &TIMESPEC_ZERO, &interval, (VoidFunc_1) _FilesystemManager_DoBgWork, self, 0));
}
