//
//  FSContainer.c
//  kernel
//
//  Created by Dietmar Planitzer on 10/12/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "FSContainer.h"
#include "FSUtilities.h"
#include <assert.h>
#include <stddef.h>
#include <ext/bit.h>
#include <kern/kernlib.h>


errno_t FSContainer_Create(Class* _Nonnull pClass, blkcnt_t blockCount, size_t blockSize, uint32_t flags, FSContainerRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    FSContainerRef self;

    assert(ispow2_sz(blockSize));

    if ((err = Object_Create(pClass, 0, (void**)&self)) == EOK) {
        self->blockCount = blockCount;
        self->blockSize = blockSize;
        self->flags = flags;
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


errno_t FSContainer_getInfo(FSContainerRef _Nonnull self, int flavor, fs_info_ref _Nonnull pOutInfo)
{
    return EINVAL;
}

errno_t FSContainer_getProperty(FSContainerRef _Nonnull self, int flavor, char* _Nonnull buf, size_t bufSize)
{
    return EINVAL;
}


class_func_defs(FSContainer, Object,
func_def(disconnect, FSContainer)
func_def(mapBlock, FSContainer)
func_def(unmapBlock, FSContainer)
func_def(prefetchBlock, FSContainer)
func_def(syncBlock, FSContainer)
func_def(sync, FSContainer)
func_def(getInfo, FSContainer)
func_def(getProperty, FSContainer)
);
