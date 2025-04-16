//
//  DiskFSContainer.c
//  kernel
//
//  Created by Dietmar Planitzer on 10/12/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "DiskFSContainer.h"
#include <diskcache/DiskCache.h>
#include <driver/DriverChannel.h>
#include <driver/disk/DiskDriver.h>
#include <filesystem/IOChannel.h>


errno_t DiskFSContainer_Create(IOChannelRef _Nonnull pChannel, FSContainerRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    struct DiskFSContainer* self = NULL;
    DiskInfo info;

    try(IOChannel_Ioctl(pChannel, kDiskCommand_GetInfo, &info));
    try(FSContainer_Create(class(DiskFSContainer), info.mediaId, info.blockCount, info.blockSize, info.isReadOnly, (FSContainerRef*)&self));

    self->channel = IOChannel_Retain(pChannel);
    self->disk = DriverChannel_GetDriverAs(pChannel, DiskDriver);
    self->diskCache = DiskDriver_GetDiskCache(self->disk);

catch:
    *pOutSelf = (FSContainerRef)self;
    return err;
}

void DiskFSContainer_deinit(struct DiskFSContainer* _Nonnull self)
{
    self->disk = NULL;
    
    IOChannel_Release(self->channel);
    self->channel = NULL;
}

errno_t DiskFSContainer_prefetchBlock(struct DiskFSContainer* _Nonnull self, LogicalBlockAddress lba)
{
    return DiskCache_PrefetchBlock(self->diskCache, self->disk, FSContainer_GetMediaId(self), lba);
}

errno_t DiskFSContainer_syncBlock(struct DiskFSContainer* _Nonnull self, LogicalBlockAddress lba)
{
    return DiskCache_SyncBlock(self->diskCache, self->disk, FSContainer_GetMediaId(self), lba);
}

errno_t DiskFSContainer_mapBlock(struct DiskFSContainer* _Nonnull self, LogicalBlockAddress lba, MapBlock mode, FSBlock* _Nonnull blk)
{
    return DiskCache_MapBlock(self->diskCache, self->disk, FSContainer_GetMediaId(self), lba, mode, blk);
}

errno_t DiskFSContainer_unmapBlock(struct DiskFSContainer* _Nonnull self, intptr_t token, WriteBlock mode)
{
    return DiskCache_UnmapBlock(self->diskCache, token, mode);
}

errno_t DiskFSContainer_sync(struct DiskFSContainer* _Nonnull self)
{
    return DiskCache_Sync(self->diskCache, self->disk, FSContainer_GetMediaId(self));
}

errno_t DiskFSContainer_getDiskName(struct DiskFSContainer* _Nonnull self, size_t bufSize, char* _Nonnull buf)
{
    return IOChannel_Ioctl(self->channel, kDriverCommand_GetCanonicalName, bufSize, buf);
}


class_func_defs(DiskFSContainer, Object,
override_func_def(deinit, DiskFSContainer, Object)
override_func_def(prefetchBlock, DiskFSContainer, FSContainer)
override_func_def(syncBlock, DiskFSContainer, FSContainer)
override_func_def(mapBlock, DiskFSContainer, FSContainer)
override_func_def(unmapBlock, DiskFSContainer, FSContainer)
override_func_def(sync, DiskFSContainer, FSContainer)
override_func_def(getDiskName, DiskFSContainer, FSContainer)
);
