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
    IOChannelRef _Nonnull   channel;
    DiskId                  diskId;
    MediaId                 mediaId;
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
        self->diskId = info->diskId;
        self->mediaId = info->mediaId;
    }

    *pOutContainer = (FSContainerRef)self;
    return err;
}

void DiskFSContainer_deinit(struct DiskFSContainer* _Nonnull self)
{
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
    return DiskCache_PrefetchBlock(gDiskCache, self->diskId, self->mediaId, lba);
}

errno_t DiskFSContainer_syncBlock(struct DiskFSContainer* _Nonnull self, LogicalBlockAddress lba)
{
    return DiskCache_SyncBlock(gDiskCache, self->diskId, self->mediaId, lba);
}

errno_t DiskFSContainer_acquireEmptyBlock(struct DiskFSContainer* self, DiskBlockRef _Nullable * _Nonnull pOutBlock)
{
    return DiskCache_AcquireEmptyBlock(gDiskCache, pOutBlock);
}

errno_t DiskFSContainer_acquireBlock(struct DiskFSContainer* _Nonnull self, LogicalBlockAddress lba, AcquireBlock mode, DiskBlockRef _Nullable * _Nonnull pOutBlock)
{
    return DiskCache_AcquireBlock(gDiskCache, self->diskId, self->mediaId, lba, mode, pOutBlock);
}

void DiskFSContainer_relinquishBlock(struct DiskFSContainer* _Nonnull self, DiskBlockRef _Nullable pBlock)
{
    DiskCache_RelinquishBlock(gDiskCache, pBlock);
}

errno_t DiskFSContainer_relinquishBlockWriting(struct DiskFSContainer* _Nonnull self, DiskBlockRef _Nullable pBlock, WriteBlock mode)
{
    return DiskCache_RelinquishBlockWriting(gDiskCache, pBlock, mode);
}

errno_t DiskFSContainer_sync(struct DiskFSContainer* _Nonnull self)
{
    return DiskCache_Sync(gDiskCache, self->diskId, self->mediaId);
}


class_func_defs(DiskFSContainer, Object,
override_func_def(deinit, DiskFSContainer, Object)
override_func_def(getInfo, DiskFSContainer, FSContainer)
override_func_def(prefetchBlock, DiskFSContainer, FSContainer)
override_func_def(syncBlock, DiskFSContainer, FSContainer)
override_func_def(acquireEmptyBlock, DiskFSContainer, FSContainer)
override_func_def(acquireBlock, DiskFSContainer, FSContainer)
override_func_def(relinquishBlock, DiskFSContainer, FSContainer)
override_func_def(relinquishBlockWriting, DiskFSContainer, FSContainer)
override_func_def(sync, DiskFSContainer, FSContainer)
);
