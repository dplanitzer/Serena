//
//  DiskBlock.c
//  kernel
//
//  Created by Dietmar Planitzer on 10/30/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "DiskBlock.h"
#include <klib/Kalloc.h>


errno_t DiskBlock_Create(DiskDriverRef _Nonnull _Weak disk, MediaId mediaId, LogicalBlockAddress lba, DiskBlockRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    DiskBlock* self = NULL;
    
    err = kalloc_cleared(sizeof(DiskBlock) + kDiskCache_BlockSize - 1, (void**) &self);
    if (err == EOK) {
        self->disk = disk;
        self->mediaId = mediaId;
        self->lba = lba;
    }

    *pOutSelf = self;
    return err;
}

void DiskBlock_Destroy(DiskBlockRef _Nullable self)
{
    if (self) {
        self->disk = NULL;
        kfree(self);
    }
}
