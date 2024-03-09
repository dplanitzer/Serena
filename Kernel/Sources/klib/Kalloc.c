//
//  Kalloc.c
//  kernel
//
//  Created by Dietmar Planitzer on 8/6/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "kalloc.h"
#include "Allocator.h"
#include <dispatcher/Lock.h>
#include "Bytes.h"


static Lock         gLock;
static AllocatorRef gUnifiedMemory;       // CPU + Chipset access (memory range [0..<chipset_upper_dma_limit])
static AllocatorRef gCpuOnlyMemory;       // CPU only access      (memory range [chipset_upper_dma_limit...])


static MemoryDescriptor adjusted_memory_descriptor(const MemoryDescriptor* pMemDesc, char* _Nonnull pInitialHeapBottom, char* _Nonnull pInitialHeapTop)
{
    MemoryDescriptor md;

    md.lower = __max(pMemDesc->lower, pInitialHeapBottom);
    md.upper = __min(pMemDesc->upper, pInitialHeapTop);
    md.type = pMemDesc->type;

    return md;
}

static errno_t create_allocator(MemoryLayout* _Nonnull pMemLayout, char* _Nonnull pInitialHeapBottom, char* _Nonnull pInitialHeapTop, int8_t memoryType, AllocatorRef _Nullable * _Nonnull pOutAllocator)
{
    int i = 0;
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
errno_t kalloc_init(const SystemDescription* _Nonnull pSysDesc, void* _Nonnull pInitialHeapBottom, void* _Nonnull pInitialHeapTop)
{
    decl_try_err();

    Lock_Init(&gLock);
    try(create_allocator(&pSysDesc->memory, pInitialHeapBottom, pInitialHeapTop, MEM_TYPE_UNIFIED_MEMORY, &gUnifiedMemory));
    try(create_allocator(&pSysDesc->memory, pInitialHeapBottom, pInitialHeapTop, MEM_TYPE_MEMORY, &gCpuOnlyMemory));
    return EOK;

catch:
    return err;
}

// Allocates memory from the kernel heap. Returns NULL if the memory could not be
// allocated. 'options' is a combination of the HEAP_ALLOC_OPTION_XXX flags.
errno_t kalloc_options(ssize_t nbytes, unsigned int options, void* _Nullable * _Nonnull pOutPtr)
{
    decl_try_err();
    
    assert(nbytes >= 0);
    
    Lock_Lock(&gLock);
    if ((options & KALLOC_OPTION_UNIFIED) != 0) {
        try(Allocator_AllocateBytes(gUnifiedMemory, nbytes, pOutPtr));
    } else {
        if (Allocator_AllocateBytes(gCpuOnlyMemory, nbytes, pOutPtr) == ENOMEM) {
            try(Allocator_AllocateBytes(gUnifiedMemory, nbytes, pOutPtr));
        }
    }
    Lock_Unlock(&gLock);

    // Zero the memory if requested
    if ((options & KALLOC_OPTION_CLEAR) != 0) {
        Bytes_ClearRange(*pOutPtr, nbytes);
    }

    return EOK;

catch:
    Lock_Unlock(&gLock);
    *pOutPtr = NULL;
    return err;
}

// Frees kernel memory allocated with the kalloc() function.
void kfree(void* _Nullable ptr)
{
    Lock_Lock(&gLock);
    const errno_t err = Allocator_DeallocateBytes(gUnifiedMemory, ptr);

    if (err == ENOTBLK) {
        try_bang(Allocator_DeallocateBytes(gCpuOnlyMemory, ptr));
    } else if (err != EOK) {
        abort();
    }
    Lock_Unlock(&gLock);
}

// Adds the given memory region as a CPU-only access memory region to the kalloc
// heap.
errno_t kalloc_add_memory_region(const MemoryDescriptor* _Nonnull pMemDesc)
{
    AllocatorRef pAllocator;

    Lock_Lock(&gLock);
    if (pMemDesc->upper < gSystemDescription->chipset_upper_dma_limit) {
        pAllocator = gUnifiedMemory;
    } else {
        pAllocator = gCpuOnlyMemory;
    }

    const errno_t err = Allocator_AddMemoryRegion(pAllocator, pMemDesc);
    Lock_Unlock(&gLock);

    return err;
}

// Dumps a description of the kalloc heap to the console
void kalloc_dump(void)
{
    Lock_Lock(&gLock);
    print("Unified:\n");
    Allocator_DumpMemoryRegions(gUnifiedMemory);

    print("\nCPU-only:\n");
    Allocator_DumpMemoryRegions(gCpuOnlyMemory);
    print("\n");
    Lock_Unlock(&gLock);
}
