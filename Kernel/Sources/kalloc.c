//
//  kalloc.c
//  Apollo
//
//  Created by Dietmar Planitzer on 8/6/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "kalloc.h"
#include "Allocator.h"
#include "Bytes.h"


static AllocatorRef   gUnifiedMemory;       // CPU + Chipset access (memory range [0..<chipset_upper_dma_limit])
static AllocatorRef   gCpuOnlyMemory;       // CPU only access      (memory range [chipset_upper_dma_limit...])


static MemoryDescriptor adjusted_memory_descriptor(const MemoryDescriptor* pMemDesc, Byte* _Nonnull pInitialHeapBottom, Byte* _Nonnull pInitialHeapTop)
{
    MemoryDescriptor md;

    md.lower = max(pMemDesc->lower, pInitialHeapBottom);
    md.upper = min(pMemDesc->upper, pInitialHeapTop);
    md.type = pMemDesc->type;

    return md;
}

static ErrorCode create_allocator(MemoryLayout* _Nonnull pMemLayout, Byte* _Nonnull pInitialHeapBottom, Byte* _Nonnull pInitialHeapTop, Int8 memoryType, AllocatorRef _Nullable * _Nonnull pOutAllocator)
{
    Int i = 0;
    AllocatorRef pAllocator = NULL;
    MemoryDescriptor adjusted_md;
    decl_try_err();

    // Skip over memory regions that are below the kernel heap bottom
    while (i < pMemLayout->descriptor_count && 
        (pMemLayout->descriptor[i].upper < pInitialHeapBottom || (pMemLayout->descriptor[i].upper >= pInitialHeapBottom && pMemLayout->descriptor[i].type != memoryType))) {
        i++;
    }
    if (i == pMemLayout->descriptor_count) {
        throw(ENOMEM);
    }


    // First valid memory descriptor. Create the allocator based on that. We'll
    // get an ENOMEM error if this memory region isn't big enough
    adjusted_md = adjusted_memory_descriptor(&pMemLayout->descriptor[i], pInitialHeapBottom, pInitialHeapTop);
    try(Allocator_Create(&adjusted_md, &pAllocator));


    // Pick up all other memory regions that are at least partially below the
    // kernel heap top
    i++;
    while (i < pMemLayout->descriptor_count && pMemLayout->descriptor[i].lower < pInitialHeapTop) {
        if (pMemLayout->descriptor[i].type == memoryType) {
            adjusted_md = adjusted_memory_descriptor(&pMemLayout->descriptor[i], pInitialHeapBottom, pInitialHeapTop);
            try(Allocator_AddMemoryRegion(pAllocator, &adjusted_md));
        }
        i++;
    }

    *pOutAllocator = pAllocator;
    return EOK;

catch:
    *pOutAllocator = NULL;
    return err;
}

// Initializes the kalloc heap.
ErrorCode kalloc_init(const SystemDescription* _Nonnull pSysDesc, Byte* _Nonnull pInitialHeapBottom, Byte* _Nonnull pInitialHeapTop)
{
    decl_try_err();

    try(create_allocator(&pSysDesc->memory, pInitialHeapBottom, pInitialHeapTop, MEM_TYPE_UNIFIED_MEMORY, &gUnifiedMemory));
    try(create_allocator(&pSysDesc->memory, pInitialHeapBottom, pInitialHeapTop, MEM_TYPE_MEMORY, &gCpuOnlyMemory));
    return EOK;

catch:
    return err;
}

// Allocates memory from the kernel heap. Returns NULL if the memory could not be
// allocated. 'options' is a combination of the HEAP_ALLOC_OPTION_XXX flags.
ErrorCode kalloc_options(Int nbytes, UInt options, Byte* _Nullable * _Nonnull pOutPtr)
{
    decl_try_err();
    
    if ((options & KALLOC_OPTION_UNIFIED) != 0) {
        try(Allocator_AllocateBytes(gUnifiedMemory, nbytes, pOutPtr));
    } else {
        if (Allocator_AllocateBytes(gCpuOnlyMemory, nbytes, pOutPtr) == ENOMEM) {
            try(Allocator_AllocateBytes(gUnifiedMemory, nbytes, pOutPtr));
        }
    }

    // Zero the memory if requested
    if ((options & KALLOC_OPTION_CLEAR) != 0) {
        Bytes_ClearRange(*pOutPtr, nbytes);
    }

    return EOK;

catch:
    *pOutPtr = NULL;
    return err;
}

// Frees kernel memory allocated with the kalloc() function.
void kfree(Byte* _Nullable ptr)
{
    if (Allocator_IsManaging(gUnifiedMemory, ptr)) {
        try_bang(Allocator_DeallocateBytes(gUnifiedMemory, ptr));
    } else {
        try_bang(Allocator_DeallocateBytes(gCpuOnlyMemory, ptr));
    }
}

// Adds the given memory region as a CPU-only access memory region to the kalloc
// heap.
ErrorCode kalloc_add_memory_region(const MemoryDescriptor* _Nonnull pMemDesc)
{
    AllocatorRef pAllocator;

    if (pMemDesc->upper < gSystemDescription->chipset_upper_dma_limit) {
        pAllocator = gUnifiedMemory;
    } else {
        pAllocator = gCpuOnlyMemory;
    }

    return Allocator_AddMemoryRegion(pAllocator, pMemDesc);
}

// Dumps a description of the kalloc heap to the console
void kalloc_dump(void)
{
    print("Unified:\n");
    Allocator_DumpMemoryRegions(gUnifiedMemory);

    print("\nCPU-only:\n");
    Allocator_DumpMemoryRegions(gCpuOnlyMemory);
    print("\n");
}
