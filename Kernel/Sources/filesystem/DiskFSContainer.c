//
//  DiskFSContainer.c
//  kernel
//
//  Created by Dietmar Planitzer on 10/12/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "DiskFSContainer.h"
#include <disk/DiskCache.h>
#include <driver/DriverCatalog.h>
#include <driver/disk/DiskDriver.h>
#include <driver/disk/DiskDriverChannel.h>
#include <filesystem/IOChannel.h>


final_class_ivars(DiskFSContainer, FSContainer,
    IOChannelRef _Nonnull           channel;
    DiskDriverRef _Nonnull _Weak    disk;
    MediaId                         mediaId;
);


errno_t DiskFSContainer_Create(IOChannelRef _Nonnull pChannel, FSContainerRef _Nullable * _Nonnull pOutContainer)
{
    decl_try_err();

    if (!instanceof(pChannel, DiskDriverChannel)) {
        *pOutContainer = NULL;
        return EINVAL;
    }

    struct DiskFSContainer* self;
    const DiskInfo* info = DiskDriverChannel_GetInfo(pChannel);

    if ((err = Object_Create(class(DiskFSContainer), 0, (void**)&self)) == EOK) {
        self->channel = IOChannel_Retain(pChannel);
        self->disk = DriverChannel_GetDriverAs(pChannel, DiskDriver);
        self->mediaId = info->mediaId;
    }

    *pOutContainer = (FSContainerRef)self;
    return err;
}

void DiskFSContainer_deinit(struct DiskFSContainer* _Nonnull self)
{
    self->disk = NULL;
    
    IOChannel_Release(self->channel);
    self->channel = NULL;
}

errno_t DiskFSContainer_getInfo(struct DiskFSContainer* _Nonnull self, FSContainerInfo* pOutInfo)
{
    const DiskInfo* info = DiskDriverChannel_GetInfo(self->channel);

    pOutInfo->isReadOnly = info->isReadOnly;
    pOutInfo->blockSize = info->blockSize;
    pOutInfo->blockCount = info->blockCount;
    
    return EOK;
}

errno_t DiskFSContainer_prefetchBlock(struct DiskFSContainer* _Nonnull self, LogicalBlockAddress lba)
{
    return DiskCache_PrefetchBlock(gDiskCache, self->disk, self->mediaId, lba);
}

errno_t DiskFSContainer_syncBlock(struct DiskFSContainer* _Nonnull self, LogicalBlockAddress lba)
{
    return DiskCache_SyncBlock(gDiskCache, self->disk, self->mediaId, lba);
}

errno_t DiskFSContainer_mapEmptyBlock(struct DiskFSContainer* self, FSBlock* _Nonnull blk)
{
    return DiskCache_MapEmptyBlock(gDiskCache, blk);
}

errno_t DiskFSContainer_mapBlock(struct DiskFSContainer* _Nonnull self, LogicalBlockAddress lba, MapBlock mode, FSBlock* _Nonnull blk)
{
    return DiskCache_MapBlock(gDiskCache, self->disk, self->mediaId, lba, mode, blk);
}

void DiskFSContainer_unmapBlock(struct DiskFSContainer* _Nonnull self, intptr_t token)
{
    DiskCache_UnmapBlock(gDiskCache, token);
}

errno_t DiskFSContainer_unmapBlockWriting(struct DiskFSContainer* _Nonnull self, intptr_t token, WriteBlock mode)
{
    return DiskCache_UnmapBlockWriting(gDiskCache, token, mode);
}

errno_t DiskFSContainer_sync(struct DiskFSContainer* _Nonnull self)
{
    return DiskCache_Sync(gDiskCache, self->disk, self->mediaId);
}


class_func_defs(DiskFSContainer, Object,
override_func_def(deinit, DiskFSContainer, Object)
override_func_def(getInfo, DiskFSContainer, FSContainer)
override_func_def(prefetchBlock, DiskFSContainer, FSContainer)
override_func_def(syncBlock, DiskFSContainer, FSContainer)
override_func_def(mapEmptyBlock, DiskFSContainer, FSContainer)
override_func_def(mapBlock, DiskFSContainer, FSContainer)
override_func_def(unmapBlock, DiskFSContainer, FSContainer)
override_func_def(unmapBlockWriting, DiskFSContainer, FSContainer)
override_func_def(sync, DiskFSContainer, FSContainer)
);
