//
//  BootAllocator.h
//  kernel
//
//  Created by Dietmar Planitzer on 8/12/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef BootAllocator_h
#define BootAllocator_h

#include <kern/types.h>
#include <hal/sys_desc.h>


typedef struct BootAllocator {
    mem_desc_t* _Nonnull  mem_descs;
    char* _Nonnull              current_top;
    int                         current_desc_index;
} BootAllocator;


extern void BootAllocator_Init(BootAllocator* _Nonnull self, sys_desc_t* _Nonnull pSysDesc);
extern void BootAllocator_Deinit(BootAllocator* _Nonnull self);

// Allocates a memory block from CPU-only RAM that is able to hold at least
// 'nbytes'. This allocator only allocates from unified memory if it can not be
// avoided. Memory is allocated top-down. Note that this function never returns
// NULL.
extern void* _Nonnull BootAllocator_Allocate(BootAllocator* _Nonnull self, size_t nbytes);

// Returns the lowest address used by the boot allocator. This address is always
// page aligned.
extern void* _Nonnull BootAllocator_GetLowestAllocatedAddress(BootAllocator* _Nonnull self);

#endif /* BootAllocator_h */
