//
//  FSContainer.c
//  kernel
//
//  Created by Dietmar Planitzer on 10/12/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "FSContainer.h"
#include "FSUtilities.h"
#include <stddef.h>


errno_t FSContainer_Create(Class* _Nonnull pClass, blkcnt_t blockCount, size_t blockSize, uint32_t properties, FSContainerRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    FSContainerRef self;

    assert(FSIsPowerOf2(blockSize));

    if ((err = Object_Create(pClass, 0, (void**)&self)) == EOK) {
        self->blockCount = blockCount;
        self->blockSize = blockSize;
        self->properties = properties;
    }

    *pOutSelf = self;
    return err;
}

void FSContainer_disconnect(FSContainerRef _Nonnull self)
{
}


errno_t FSContainer_mapBlock(FSContainerRef _Nonnull self, blkno_t lba, MapBlock mode, FSBlock* _Nonnull blk)
{
    blk->token = 0;
    blk->data = NULL;

    return EIO;
}

errno_t FSContainer_unmapBlock(FSContainerRef _Nonnull self, intptr_t token, WriteBlock mode)
{
    return EIO;
}

errno_t FSContainer_prefetchBlock(FSContainerRef _Nonnull self, blkno_t lba)
{
    return EOK;
}


errno_t FSContainer_syncBlock(FSContainerRef _Nonnull self, blkno_t lba)
{
    return EOK;
}

errno_t FSContainer_sync(FSContainerRef _Nonnull self)
{
    return EOK;
}

errno_t FSContainer_getDiskInfo(FSContainerRef _Nonnull self, disk_info_t* _Nonnull info)
{
    return ENOTSUP;
}


class_func_defs(FSContainer, Object,
func_def(disconnect, FSContainer)
func_def(mapBlock, FSContainer)
func_def(unmapBlock, FSContainer)
func_def(prefetchBlock, FSContainer)
func_def(syncBlock, FSContainer)
func_def(sync, FSContainer)
func_def(getDiskInfo, FSContainer)
);
