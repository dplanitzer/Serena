//
//  FilesystemManager.c
//  kernel
//
//  Created by Dietmar Planitzer on 12/12/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "FilesystemManager.h"
#include <Catalog.h>
#include <diskcache/DiskCache.h>
#include <dispatcher/Lock.h>
#include <dispatchqueue/DispatchQueue.h>
#include <driver/disk/DiskDriver.h>
#include <filesystem/DiskFSContainer.h>
#include <filesystem/IOChannel.h>
#include <filesystem/serenafs/SerenaFS.h>
#include <klib/List.h>


typedef struct ReaperEntry {
    ListNode                node;
    FilesystemRef _Nonnull  fs;
} ReaperEntry;


typedef struct FilesystemManager {
    DispatchQueueRef _Nonnull   dispatchQueue;
    Lock                        reaperLock;
    List                        reaperQueue;
} FilesystemManager;


static void _FilesystemManager_ScheduleAutoSync(FilesystemManagerRef _Nonnull self);



FilesystemManagerRef _Nonnull gFilesystemManager;

errno_t FilesystemManager_Create(FilesystemManagerRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    FilesystemManagerRef self;

    try(kalloc_cleared(sizeof(FilesystemManager), (void**)&self));
    try(DispatchQueue_Create(0, 1, kDispatchQoS_Background, 0, gVirtualProcessorPool, NULL, (DispatchQueueRef*)&self->dispatchQueue));
    Lock_Init(&self->reaperLock);

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

    try(DiskFSContainer_Create(driverChannel, &fsContainer));
    try(SerenaFS_Create(fsContainer, (SerenaFSRef*)&fs));
    try(Filesystem_Start(fs, params, paramsSize));
    try(Filesystem_Publish(fs));

    Object_Release(fsContainer);
    *pOutFs = fs;
    return EOK;

catch:
    Object_Release(fs);
    Object_Release(fsContainer);

    return err;
}

bool FilesystemManager_StopFilesystem(FilesystemManagerRef _Nonnull self, FilesystemRef _Nonnull fs, bool force)
{
    decl_try_err();

    err = Filesystem_Stop(fs);
    if (err == EBUSY) {
        if (force) {
            ReaperEntry* re;

            Filesystem_Unpublish(fs);

            try_bang(kalloc_cleared(sizeof(ReaperEntry), (void**)&re));
            re->fs = Object_RetainAs(fs, Filesystem);

            Lock_Lock(&self->reaperLock);
            List_InsertAfterLast(&self->reaperQueue, &re->node);
            Lock_Unlock(&self->reaperLock);

            return true;
        }
        else {
            return false;
        }
    }
    else if (err == EATTACHED) {
        // Still attached somewhere else. Don't unpublish
        return false;
    }
    else {
        // Not attached anywhere else anymore and stop has worked. Unpublish
        Filesystem_Unpublish(fs);
        return true;
    }
}

void FilesystemManager_Sync(FilesystemManagerRef _Nonnull self)
{
    DiskCache_Sync(gDiskCache, NULL, kMediaId_Current);
}

// Tries to stop and destroy filesystems that are on the reaper queue.
static void _FilesystemManager_Reaper(FilesystemManagerRef _Nonnull self)
{
    List queue;

    // Take a snapshot of the reaper queue so that we can do our work without
    // having to hold the lock.
    Lock_Lock(&self->reaperLock);
    queue = self->reaperQueue;
    List_Init(&self->reaperQueue);
    Lock_Unlock(&self->reaperLock);

    
    // Kill as many filesystems as we can
    if (!List_IsEmpty(&queue)) {

        List_ForEach(&queue, ReaperEntry,
            if (Filesystem_Stop(pCurNode->fs) == EOK) {
                List_Remove(&queue, &pCurNode->node);
                Object_Release(pCurNode->fs);
                pCurNode->fs = NULL;
                kfree(pCurNode);
            }
        );


        // Put the rest back on the reaper queue
        Lock_Lock(&self->reaperLock);
        List_ForEach(&queue, ReaperEntry,
            List_Remove(&queue, &pCurNode->node);
            List_InsertBeforeFirst(&self->reaperQueue, &pCurNode->node);
        );
        Lock_Unlock(&self->reaperLock);
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
