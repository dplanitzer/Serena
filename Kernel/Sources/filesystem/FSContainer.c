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

errno_t FSContainer_syncBlock(FSContainerRef _Nonnull self, LogicalBlockAddress lba)
{
    return EOK;
}

errno_t FSContainer_mapBlock(FSContainerRef _Nonnull self, LogicalBlockAddress lba, MapBlock mode, FSBlock* _Nonnull blk)
{
    blk->token = 0;
    blk->data = NULL;

    return EIO;
}

void FSContainer_unmapBlock(FSContainerRef _Nonnull self, intptr_t token)
{
}

errno_t FSContainer_unmapBlockWriting(FSContainerRef _Nonnull self, intptr_t token, WriteBlock mode)
{
    return EIO;
}

errno_t FSContainer_sync(FSContainerRef _Nonnull self)
{
    return EOK;
}


class_func_defs(FSContainer, Object,
func_def(getInfo, FSContainer)
func_def(prefetchBlock, FSContainer)
func_def(syncBlock, FSContainer)
func_def(mapBlock, FSContainer)
func_def(unmapBlock, FSContainer)
func_def(unmapBlockWriting, FSContainer)
func_def(sync, FSContainer)
);
