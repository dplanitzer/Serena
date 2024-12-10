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
#include <filesystem/IOChannel.h>


final_class_ivars(DiskFSContainer, FSContainer,
    IOChannelRef _Nonnull   channel;
    DriverId                diskId;
    MediaId                 mediaId;
);


errno_t DiskFSContainer_Create(IOChannelRef _Nonnull pChannel, DriverId diskId, MediaId mediaId, FSContainerRef _Nullable * _Nonnull pOutContainer)
{
    decl_try_err();
    struct DiskFSContainer* self = NULL;
    FSContainerInfo info;

    if ((err = Object_Create(DiskFSContainer, (struct DiskFSContainer*)&self)) == EOK) {
        self->channel = IOChannel_Retain(pChannel);
        self->diskId = diskId;
        self->mediaId = mediaId;
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
    decl_try_err();
    DiskDriverRef pDriver;
    DiskInfo di;

    if ((pDriver = (DiskDriverRef) DriverCatalog_CopyDriverForDriverId(gDriverCatalog, self->diskId)) != NULL) {
        if ((err = DiskDriver_GetInfo(pDriver, &di)) == EOK) {
            pOutInfo->blockSize = di.blockSize;
            pOutInfo->blockCount = di.blockCount;
            pOutInfo->mediaId = di.mediaId;
            pOutInfo->isReadOnly = di.isReadOnly;
        }
        Object_Release(pDriver);
    }
    else {
        err = ENODEV;
    }

    return err;
}

// Prefetches a block and stores it in the disk cache if possible. The prefetch
// is executed asynchronously. An error is returned if the prefetch could not
// be successfully started. Note that the returned error does not indicate
// whether the read operation as such was successful or not.
errno_t DiskFSContainer_prefetchBlock(struct DiskFSContainer* _Nonnull self, DriverId driverId, MediaId mediaId, LogicalBlockAddress lba)
{
    return DiskCache_PrefetchBlock(gDiskCache, driverId, mediaId, lba);
}

// Flushes the block at disk address (driverId, mediaId, lba) to disk if it
// contains unwritten (dirty) data. Does nothing if the block is clean.
errno_t DiskFSContainer_flushBlock(struct DiskFSContainer* _Nonnull self, DriverId driverId, MediaId mediaId, LogicalBlockAddress lba)
{
    return DiskCache_FlushBlock(gDiskCache, driverId, mediaId, lba);
}

// Acquires an empty block, filled with zero bytes. This block is not attached
// to any disk address and thus may not be written back to disk.
errno_t DiskFSContainer_acquireEmptyBlock(struct DiskFSContainer* self, DiskBlockRef _Nullable * _Nonnull pOutBlock)
{
    return DiskCache_AcquireEmptyBlock(gDiskCache, pOutBlock);
}

// Acquires the disk block with the block address 'lba'. The acquisition is
// done according to the acquisition mode 'mode'. An error is returned if
// the disk block needed to be loaded and loading failed for some reason.
// Once done with the block, it must be relinquished by calling the
// relinquishBlock() method.
errno_t DiskFSContainer_acquireBlock(struct DiskFSContainer* _Nonnull self, LogicalBlockAddress lba, AcquireBlock mode, DiskBlockRef _Nullable * _Nonnull pOutBlock)
{
    return DiskCache_AcquireBlock(gDiskCache, self->diskId, self->mediaId, lba, mode, pOutBlock);
}

// Relinquishes the disk block 'pBlock' without writing it back to disk.
void DiskFSContainer_relinquishBlock(struct DiskFSContainer* _Nonnull self, DiskBlockRef _Nullable pBlock)
{
    DiskCache_RelinquishBlock(gDiskCache, pBlock);
}

// Relinquishes the disk block 'pBlock' and writes the disk block back to
// disk according to the write back mode 'mode'.
errno_t DiskFSContainer_relinquishBlockWriting(struct DiskFSContainer* _Nonnull self, DiskBlockRef _Nullable pBlock, WriteBlock mode)
{
    return DiskCache_RelinquishBlockWriting(gDiskCache, pBlock, mode);
}

// Flushes all cached and unwritten blocks belonging to this FS container to
// disk(s).
errno_t DiskFSContainer_flush(struct DiskFSContainer* _Nonnull self)
{
    return DiskCache_Flush(gDiskCache, self->diskId, self->mediaId);
}


class_func_defs(DiskFSContainer, Object,
override_func_def(deinit, DiskFSContainer, Object)
override_func_def(getInfo, DiskFSContainer, FSContainer)
override_func_def(prefetchBlock, DiskFSContainer, FSContainer)
override_func_def(flushBlock, DiskFSContainer, FSContainer)
override_func_def(acquireEmptyBlock, DiskFSContainer, FSContainer)
override_func_def(acquireBlock, DiskFSContainer, FSContainer)
override_func_def(relinquishBlock, DiskFSContainer, FSContainer)
override_func_def(relinquishBlockWriting, DiskFSContainer, FSContainer)
override_func_def(flush, DiskFSContainer, FSContainer)
);
