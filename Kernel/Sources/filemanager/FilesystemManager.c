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
#include <dispatchqueue/DispatchQueue.h>
#include <driver/disk/DiskDriver.h>
#include <filesystem/DiskFSContainer.h>
#include <filesystem/IOChannel.h>
#include <filesystem/serenafs/SerenaFS.h>


typedef struct FilesystemManager {
    DispatchQueueRef _Nonnull   autoSyncQueue;
} FilesystemManager;

static void _FilesystemManager_ScheduleAutoSync(FilesystemManagerRef _Nonnull self);



FilesystemManagerRef _Nonnull gFilesystemManager;

errno_t FilesystemManager_Create(FilesystemManagerRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    FilesystemManagerRef self;

    try(kalloc_cleared(sizeof(FilesystemManager), (void**)&self));
    try(DispatchQueue_Create(0, 1, kDispatchQoS_Background, 0, gVirtualProcessorPool, NULL, (DispatchQueueRef*)&self->autoSyncQueue));

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
        // XXX add support for forced stop
        return false;
    }
    else {
        Filesystem_Unpublish(fs);
        return true;
    }
}

// Auto syncs cache blocks to their associated disks
static void _FilesystemManager_AutoSync(FilesystemManagerRef _Nonnull self)
{
    DiskCache_Sync(gDiskCache, NULL, kMediaId_Current);
}

// Schedule an automatic sync of cached blocks to the disk(s)
static void _FilesystemManager_ScheduleAutoSync(FilesystemManagerRef _Nonnull self)
{
    try_bang(DispatchQueue_DispatchAsyncPeriodically(self->autoSyncQueue, kTimeInterval_Zero, TimeInterval_MakeSeconds(30), (VoidFunc_1) _FilesystemManager_AutoSync, self, 0));
}
