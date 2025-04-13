//
//  FilesystemManager.c
//  kernel
//
//  Created by Dietmar Planitzer on 12/12/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "FilesystemManager.h"
#include <driver/DriverCatalog.h>
#include <driver/disk/DiskDriver.h>
#include <filesystem/DiskFSContainer.h>
#include <filesystem/IOChannel.h>
#include <filesystem/serenafs/SerenaFS.h>


typedef struct FilesystemManager {
    int dummy;
} FilesystemManager;


FilesystemManagerRef _Nonnull gFilesystemManager;

errno_t FilesystemManager_Create(FilesystemManagerRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    FilesystemManagerRef self;

    try(kalloc_cleared(sizeof(FilesystemManager), (void**)&self));

catch:
    *pOutSelf = self;
    return err;
}

errno_t FilesystemManager_DiscoverAndStartFilesystem(FilesystemManagerRef _Nonnull self, const char* _Nonnull driverPath, const void* _Nullable params, size_t paramsSize, FilesystemRef _Nullable * _Nonnull pOutFs)
{
    decl_try_err();
    IOChannelRef chan = NULL;

    if ((err = DriverCatalog_OpenDriver(gDriverCatalog, driverPath, kOpen_ReadWrite, &chan)) == EOK) {
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
