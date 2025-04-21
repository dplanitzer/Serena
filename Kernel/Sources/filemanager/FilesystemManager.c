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
#include <filesystem/DiskContainer.h>
#include <filesystem/IOChannel.h>
#include <filesystem/serenafs/SerenaFS.h>
#include <klib/List.h>


typedef struct FSEntry {
    ListNode                node;
    FilesystemRef _Nonnull  fs;
} FSEntry;


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


static errno_t _FilesystemManager_StartFilesystem(FilesystemManagerRef _Nonnull self, FilesystemRef _Nonnull fs, const void* _Nullable params, size_t paramsSize);
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

errno_t FilesystemManager_DiscoverAndStartFilesystem(FilesystemManagerRef _Nonnull self, const char* _Nonnull driverPath, const void* _Nullable params, size_t paramsSize, FilesystemRef _Nullable * _Nonnull pOutFs)
{
    decl_try_err();
    IOChannelRef chan = NULL;

    if ((err = Catalog_Open(gDriverCatalog, driverPath, kOpen_ReadWrite, &chan)) == EOK) {
        err = FilesystemManager_DiscoverAndStartFilesystemWithChannel(self, chan, params, paramsSize, pOutFs);
    } 
    IOChannel_Release(chan);
    
    return err;
}

errno_t FilesystemManager_DiscoverAndStartFilesystemWithChannel(FilesystemManagerRef _Nonnull self, IOChannelRef _Nonnull driverChannel, const void* _Nullable params, size_t paramsSize, FilesystemRef _Nullable * _Nonnull pOutFs)
{
    decl_try_err();
    FSContainerRef fsContainer = NULL;
    FilesystemRef fs = NULL;

    try(DiskContainer_Create(driverChannel, &fsContainer));
    try(SerenaFS_Create(fsContainer, (SerenaFSRef*)&fs));
    try(_FilesystemManager_StartFilesystem(self, fs, params, paramsSize));
    Object_Release(fsContainer);

    *pOutFs = fs;
    return EOK;

catch:
    Object_Release(fs);
    Object_Release(fsContainer);

    return err;
}

static errno_t _FilesystemManager_StartFilesystem(FilesystemManagerRef _Nonnull self, FilesystemRef _Nonnull fs, const void* _Nullable params, size_t paramsSize)
{
    decl_try_err();
    FSEntry* entry = NULL;

    try(kalloc_cleared(sizeof(FSEntry), (void**)&entry));
    try(Filesystem_Start(fs, params, paramsSize));
    try(Filesystem_Publish(fs));

    entry->fs = Object_RetainAs(fs, Filesystem);

    Lock_Lock(&self->lock);
    List_InsertAfterLast(&self->filesystems, &entry->node);
    Lock_Unlock(&self->lock);
    return EOK;

catch:
    kfree(entry);
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
            FSEntry* ep = NULL;
            List_ForEach(&self->filesystems, FSEntry,
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
    else if (err == EATTACHED) {
        // Still attached somewhere else. Don't unpublish
        return EATTACHED;
    }
    else {
        // Not attached anywhere else anymore and stop has worked. Unpublish
        Filesystem_Unpublish(fs);

        Lock_Lock(&self->lock);
        FSEntry* ep = NULL;
        List_ForEach(&self->filesystems, FSEntry,
            if (pCurNode->fs == fs) {
                ep = pCurNode;
                break;
            }
        );
        assert(ep != NULL);

        Object_Release(ep->fs);
        ep->fs = NULL;
        List_Remove(&self->filesystems, &ep->node);
        Lock_Unlock(&self->lock);

        return err;
    }
}

void FilesystemManager_Sync(FilesystemManagerRef _Nonnull self)
{
    Lock_Lock(&self->lock);
    // XXX change this to run the syncs outside of the lock
    List_ForEach(&self->filesystems, FSEntry,
        Filesystem_Sync(pCurNode->fs);
    );
    Lock_Unlock(&self->lock);
}

static void _FilesystemManager_ReapFilesystem(FilesystemManagerRef _Nonnull self, List* _Nonnull queue, FSEntry* _Nonnull ep)
{
    List_Remove(queue, &ep->node);
    Object_Release(ep->fs);
    ep->fs = NULL;
    kfree(ep);
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

        List_ForEach(&queue, FSEntry,
            if (Filesystem_CanDestroy(pCurNode->fs)) {
                _FilesystemManager_ReapFilesystem(self, &queue, pCurNode);
            }
        );


        // Put the rest back on the reaper queue
        Lock_Lock(&self->lock);
        List_ForEach(&queue, FSEntry,
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
