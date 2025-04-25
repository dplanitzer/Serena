//
//  FilesystemManager.c
//  kernel
//
//  Created by Dietmar Planitzer on 12/12/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "FilesystemManager.h"
#include <Catalog.h>
#include <dispatcher/Lock.h>
#include <dispatchqueue/DispatchQueue.h>
#include <driver/disk/DiskDriver.h>
#include <filesystem/IOChannel.h>
#include <klib/List.h>


typedef struct fsentry {
    ListNode                node;
    FilesystemRef _Nonnull  fs;
    char* _Nonnull          diskPath;
} fsentry_t;

static void fsentry_destroy(fsentry_t* _Nullable self)
{
    if (self) {
        Object_Release(self->fs);
        self->fs = NULL;

        kfree(self->diskPath);
        self->diskPath = NULL;

        kfree(self);    
    }
}

static errno_t fsentry_create(FilesystemRef _Nonnull fs, const char* _Nonnull diskPath, fsentry_t* _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    const size_t len = String_Length(diskPath);
    fsentry_t* self;

    try(kalloc_cleared(sizeof(fsentry_t), (void**)&self));
    try(kalloc(len + 1, (void**)&self->diskPath));
    memcpy(self->diskPath, diskPath, len + 1);
    self->fs = Object_RetainAs(fs, Filesystem);

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
    Lock                        lock;
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
    try(DispatchQueue_Create(0, 1, kDispatchQoS_Background, 0, gVirtualProcessorPool, NULL, (DispatchQueueRef*)&self->dispatchQueue));
    Lock_Init(&self->lock);

    _FilesystemManager_ScheduleAutoSync(self);

catch:
    *pOutSelf = self;
    return err;
}

errno_t FilesystemManager_StartFilesystem(FilesystemManagerRef _Nonnull self, FilesystemRef _Nonnull fs, const char* _Nonnull params, const char* _Nonnull diskPath)
{
    decl_try_err();
    fsentry_t* entry = NULL;

    try(Filesystem_Start(fs, params));
    try(Filesystem_Publish(fs));
    try(fsentry_create(fs, diskPath, &entry));

    Lock_Lock(&self->lock);
    List_InsertAfterLast(&self->filesystems, &entry->node);
    Lock_Unlock(&self->lock);
    return EOK;

catch:
    if (fs) {
        Filesystem_Unpublish(fs);
        Filesystem_Stop(fs, false);
    }
    return err;
}

errno_t FilesystemManager_StopFilesystem(FilesystemManagerRef _Nonnull self, FilesystemRef _Nonnull fs, bool forced)
{
    decl_try_err();

    err = Filesystem_Stop(fs, forced);
    if (err == EBUSY) {
        if (forced) {
            Filesystem_Unpublish(fs);

            Lock_Lock(&self->lock);
            fsentry_t* ep = NULL;
            List_ForEach(&self->filesystems, fsentry_t,
                if (pCurNode->fs == fs) {
                    ep = pCurNode;
                    break;
                }
            );
            assert(ep != NULL);

            List_Remove(&self->filesystems, &ep->node);
            List_InsertAfterLast(&self->reaperQueue, &ep->node);
            Lock_Unlock(&self->lock);

            return EOK;
        }
        else {
            return EBUSY;
        }
    }
    else {
        // Not attached anywhere else anymore and stop has worked. Unpublish
        Filesystem_Unpublish(fs);

        Lock_Lock(&self->lock);
        fsentry_t* ep = NULL;
        List_ForEach(&self->filesystems, fsentry_t,
            if (pCurNode->fs == fs) {
                ep = pCurNode;
                break;
            }
        );
        assert(ep != NULL);

        List_Remove(&self->filesystems, &ep->node);
        Lock_Unlock(&self->lock);
        fsentry_destroy(ep);

        return err;
    }
}

void FilesystemManager_Sync(FilesystemManagerRef _Nonnull self)
{
    Lock_Lock(&self->lock);
    // XXX change this to run the syncs outside of the lock
    List_ForEach(&self->filesystems, fsentry_t,
        Filesystem_Sync(pCurNode->fs);
    );
    Lock_Unlock(&self->lock);
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
    Lock_Lock(&self->lock);
    queue = self->reaperQueue;
    List_Init(&self->reaperQueue);
    Lock_Unlock(&self->lock);

    
    // Kill as many filesystems as we can
    if (!List_IsEmpty(&queue)) {

        List_ForEach(&queue, fsentry_t,
            if (Filesystem_CanDestroy(pCurNode->fs)) {
                _FilesystemManager_ReapFilesystem(self, &queue, pCurNode);
            }
        );


        // Put the rest back on the reaper queue
        Lock_Lock(&self->lock);
        List_ForEach(&queue, fsentry_t,
            List_Remove(&queue, &pCurNode->node);
            List_InsertBeforeFirst(&self->reaperQueue, &pCurNode->node);
        );
        Lock_Unlock(&self->lock);
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
    try_bang(DispatchQueue_DispatchAsyncPeriodically(self->dispatchQueue, kTimeInterval_Zero, TimeInterval_MakeSeconds(30), (VoidFunc_1) _FilesystemManager_DoBgWork, self, 0));
}
