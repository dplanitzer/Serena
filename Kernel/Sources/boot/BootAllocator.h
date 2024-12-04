//
//  BootAllocator.h
//  kernel
//
//  Created by Dietmar Planitzer on 8/12/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef BootAllocator_h
#define BootAllocator_h

#include <klib/klib.h>
#include <hal/SystemDescription.h>


typedef struct BootAllocator {
    MemoryDescriptor* _Nonnull  mem_descs;
    char* _Nonnull              current_top;
    int                         current_desc_index;
} BootAllocator;


extern void BootAllocator_Init(BootAllocator* _Nonnull pAlloc, SystemDescription* _Nonnull pSysDesc);

extern void BootAllocator_Deinit(BootAllocator* _Nonnull pAlloc);

// Allocates a memory block from CPU-only RAM that is able to hold at least
// 'nbytes'. This allocator only allocates from unified memory if it can not be
// avoided. Note that the base address of the allocated block is page aligned.
// Never returns NULL. Memory is allocated top-down.
extern void* _Nonnull BootAllocator_Allocate(BootAllocator* _Nonnull pAlloc, ssize_t nbytes);

// Returns the lowest address used by the boot allocator. This address is always
// page aligned.
extern void* _Nonnull BootAllocator_GetLowestAllocatedAddress(BootAllocator* _Nonnull pAlloc);

#endif /* BootAllocator_h */
