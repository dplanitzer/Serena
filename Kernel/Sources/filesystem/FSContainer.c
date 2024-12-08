//
//  FSContainer.c
//  kernel
//
//  Created by Dietmar Planitzer on 10/12/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "FSContainer.h"


// Returns information about the filesystem container and the underlying disk
// medium(s).
errno_t FSContainer_getInfo(FSContainerRef _Nonnull self, FSContainerInfo* pOutInfo)
{
    pOutInfo->blockSize = 512;
    pOutInfo->blockCount = 0;
    pOutInfo->mediaId = 0;
    pOutInfo->isReadOnly = true;

    return EOK;
}

// Prefetches a block and stores it in the disk cache if possible. The prefetch
// is executed asynchronously. An error is returned if the prefetch could not
// be successfully started. Note that the returned error does not indicate
// whether the read operation as such was successful or not.
errno_t FSContainer_prefetchBlock(FSContainerRef _Nonnull self, DriverId driverId, MediaId mediaId, LogicalBlockAddress lba)
{
    return EIO;
}

// Flushes the block at disk address (driverId, mediaId, lba) to disk if it
// contains unwritten (dirty) data. Does nothing if the block is clean.
errno_t FSContainer_flushBlock(FSContainerRef _Nonnull self, DriverId driverId, MediaId mediaId, LogicalBlockAddress lba)
{
    return EOK;
}

// Acquires an empty block, filled with zero bytes. This block is not attached
// to any disk address and thus may not be written back to disk.
errno_t FSContainer_acquireEmptyBlock(FSContainerRef self, DiskBlockRef _Nullable * _Nonnull pOutBlock)
{
    return EIO;
}

// Acquires the disk block with the block address 'lba'. The acquisition is
// done according to the acquisition mode 'mode'. An error is returned if
// the disk block needed to be loaded and loading failed for some reason.
// Once done with the block, it must be relinquished by calling the
// relinquishBlock() method.
errno_t FSContainer_acquireBlock(FSContainerRef _Nonnull self, LogicalBlockAddress lba, AcquireBlock mode, DiskBlockRef _Nullable * _Nonnull pOutBlock)
{
    return EIO;
}

// Relinquishes the disk block 'pBlock' without writing it back to disk.
void FSContainer_relinquishBlock(FSContainerRef _Nonnull self, DiskBlockRef _Nullable pBlock)
{
}

// Relinquishes the disk block 'pBlock' and writes the disk block back to
// disk according to the write back mode 'mode'.
errno_t FSContainer_relinquishBlockWriting(FSContainerRef _Nonnull self, DiskBlockRef _Nullable pBlock, WriteBlock mode)
{
    return EIO;
}

// Flushes all cached and unwritten blocks belonging to this FS container to
// disk(s).
errno_t FSContainer_flush(FSContainerRef _Nonnull self)
{
    return EOK;
}


class_func_defs(FSContainer, Object,
func_def(getInfo, FSContainer)
func_def(prefetchBlock, FSContainer)
func_def(flushBlock, FSContainer)
func_def(acquireEmptyBlock, FSContainer)
func_def(acquireBlock, FSContainer)
func_def(relinquishBlock, FSContainer)
func_def(relinquishBlockWriting, FSContainer)
func_def(flush, FSContainer)
);
