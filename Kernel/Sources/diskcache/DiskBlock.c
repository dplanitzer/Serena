//
//  DiskBlock.c
//  kernel
//
//  Created by Dietmar Planitzer on 10/30/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "DiskBlock.h"
#include <klib/Kalloc.h>


errno_t DiskBlock_Create(int sessionId, bno_t lba, size_t blockSize, DiskBlockRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    DiskBlock* self = NULL;
    
    err = kalloc_cleared(sizeof(DiskBlock) + blockSize - 1, (void**) &self);
    if (err == EOK) {
        self->sessionId = sessionId;
        self->lba = lba;
    }

    *pOutSelf = self;
    return err;
}

void DiskBlock_Destroy(DiskBlockRef _Nullable self)
{
    kfree(self);
}
