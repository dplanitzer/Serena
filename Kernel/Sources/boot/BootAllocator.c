//
//  BootAllocator.c
//  kernel
//
//  Created by Dietmar Planitzer on 8/12/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "BootAllocator.h"
#include <assert.h>
#include <kern/kernlib.h>
#include <hal/cpu.h>

#define __MEM_ALIGN 4


void BootAllocator_Init(BootAllocator* _Nonnull self, sys_desc_t* _Nonnull pSysDesc)
{
    assert(pSysDesc->motherboard_ram.desc_count > 0);
    self->mem_descs = pSysDesc->motherboard_ram.desc;
    self->current_desc_index = pSysDesc->motherboard_ram.desc_count - 1;
    self->current_top = __Floor_Ptr_PowerOf2(self->mem_descs[self->current_desc_index].upper, __MEM_ALIGN);
}

void BootAllocator_Deinit(BootAllocator* _Nonnull self)
{
    self->mem_descs = NULL;
    self->current_top = NULL;
}

void* _Nonnull BootAllocator_Allocate(BootAllocator* _Nonnull self, size_t nbytes)
{
    assert(nbytes > 0);
    char* ptr = NULL;

    while(true) {
        ptr = __Floor_Ptr_PowerOf2(self->current_top - nbytes, __MEM_ALIGN);
        if (ptr >= self->mem_descs[self->current_desc_index].lower) {
            self->current_top = ptr;
            return ptr;
        }

        assert(self->current_desc_index > 0);
        self->current_desc_index--;
        self->current_top = __Floor_Ptr_PowerOf2(self->mem_descs[self->current_desc_index].upper, __MEM_ALIGN);
    }
}

void* _Nonnull BootAllocator_GetLowestAllocatedAddress(BootAllocator* _Nonnull self)
{
    return self->current_top;
}
