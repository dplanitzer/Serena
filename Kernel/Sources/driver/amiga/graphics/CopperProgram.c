//
//  CopperProgram.c
//  kernel
//
//  Created by Dietmar Planitzer on 9/24/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "CopperProgram.h"
#include <kern/kalloc.h>


errno_t CopperProgram_Create(size_t instrCount, CopperProgram* _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    CopperProgram* self;

    err = kalloc_options(sizeof(CopperProgram) + (instrCount - 1) * sizeof(CopperInstruction), KALLOC_OPTION_UNIFIED, (void**) &self);
    if (err == EOK) {
        SListNode_Init(&self->node);
    }
    
    *pOutSelf = self;
    return err;
}

// Frees the given Copper program.
void CopperProgram_Destroy(CopperProgram* _Nullable self)
{
    kfree(self);
}
