//
//  DiskBlock.c
//  kernel
//
//  Created by Dietmar Planitzer on 10/30/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "DiskBlock.h"
#include <klib/Kalloc.h>

#define BLOCK_SIZE  512


errno_t DiskBlock_Create(DiskId diskId, MediaId mediaId, LogicalBlockAddress lba, DiskBlockRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    DiskBlock* self;
    
    try(kalloc_cleared(sizeof(DiskBlock) + BLOCK_SIZE - 1, (void**) &self));
    ListNode_Init(&self->hashNode);
    ListNode_Init(&self->lruNode);
    self->diskId = diskId;
    self->mediaId = mediaId;
    self->lba = lba;
    self->flags.byteSize = BLOCK_SIZE;

    *pOutSelf = self;
    return EOK;

catch:
    DiskBlock_Destroy(self);
    *pOutSelf = NULL;
    return err;
}

void DiskBlock_Destroy(DiskBlockRef _Nullable self)
{
    if (self) {
        ListNode_Deinit(&self->hashNode);
        ListNode_Deinit(&self->lruNode);

        kfree(self);
    }
}
