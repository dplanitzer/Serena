//
//  FSContainer.c
//  kernel
//
//  Created by Dietmar Planitzer on 10/12/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "FSContainer.h"


errno_t FSContainer_getInfo(FSContainerRef _Nonnull self, FSContainerInfo* pOutInfo)
{
    pOutInfo->blockSize = 512;
    pOutInfo->blockCount = 0;
    pOutInfo->isReadOnly = true;

    return EOK;
}

errno_t FSContainer_prefetchBlock(FSContainerRef _Nonnull self, LogicalBlockAddress lba)
{
    return EOK;
}

errno_t FSContainer_flushBlock(FSContainerRef _Nonnull self, LogicalBlockAddress lba)
{
    return EOK;
}

errno_t FSContainer_acquireEmptyBlock(FSContainerRef self, DiskBlockRef _Nullable * _Nonnull pOutBlock)
{
    return EIO;
}

errno_t FSContainer_acquireBlock(FSContainerRef _Nonnull self, LogicalBlockAddress lba, AcquireBlock mode, DiskBlockRef _Nullable * _Nonnull pOutBlock)
{
    return EIO;
}

void FSContainer_relinquishBlock(FSContainerRef _Nonnull self, DiskBlockRef _Nullable pBlock)
{
}

errno_t FSContainer_relinquishBlockWriting(FSContainerRef _Nonnull self, DiskBlockRef _Nullable pBlock, WriteBlock mode)
{
    return EIO;
}

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
