//
//  Kalloc.c
//  kernel
//
//  Created by Dietmar Planitzer on 8/6/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "kalloc.h"
#include "Allocator.h"
#include "Memory.h"
#include <dispatcher/Lock.h>


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
    decl_try_err();
    int i = 0;
    AllocatorRef pAllocator = NULL;
    MemoryDescriptor adjusted_md;

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
    try(create_allocator(&pSysDesc->motherboard_ram, pInitialHeapBottom, pInitialHeapTop, MEM_TYPE_UNIFIED_MEMORY, &gUnifiedMemory));
    try(create_allocator(&pSysDesc->motherboard_ram, pInitialHeapBottom, pInitialHeapTop, MEM_TYPE_MEMORY, &gCpuOnlyMemory));
    return EOK;

catch:
    return err;
}

// Allocates memory from the kernel heap. Returns NULL if the memory could not be
// allocated. 'options' is a combination of the HEAP_ALLOC_OPTION_XXX flags.
errno_t kalloc_options(size_t nbytes, unsigned int options, void* _Nullable * _Nonnull pOutPtr)
{
    decl_try_err();
    
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
        memset(*pOutPtr, 0, nbytes);
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
    decl_try_err();

    Lock_Lock(&gLock);
    err = Allocator_DeallocateBytes(gUnifiedMemory, ptr);

    if (err == ENOTBLK) {
        //try_bang(Allocator_DeallocateBytes(gCpuOnlyMemory, ptr));
        // XXX Early boot code (kfree() in InterruptController_AddInterruptHandler, called from the KeyboardDriver_Start) calls kfree with what appears to be an invalid pointer that triggers a ENOTBLK
        Allocator_DeallocateBytes(gCpuOnlyMemory, ptr);
    } else if (err != EOK) {
        abort();
    }
    Lock_Unlock(&gLock);
}

// Returns the gross size of the given memory block. The gross size may be a bit
// bigger than what was originally requested, because of alignment constraints.
size_t ksize(void* _Nullable ptr)
{
    decl_try_err();
    size_t nbytes = 0;

    Lock_Lock(&gLock);
    err = Allocator_GetBlockSize(gUnifiedMemory, ptr, &nbytes);

    if (err == ENOTBLK) {
        err = Allocator_GetBlockSize(gCpuOnlyMemory, ptr, &nbytes);
    }

    Lock_Unlock(&gLock);

    return nbytes;
}

// Adds the given memory region as a CPU-only access memory region to the kalloc
// heap.
errno_t kalloc_add_memory_region(const MemoryDescriptor* _Nonnull pMemDesc)
{
    decl_try_err();
    AllocatorRef pAllocator;

    Lock_Lock(&gLock);
    if (pMemDesc->upper < gSystemDescription->chipset_upper_dma_limit) {
        pAllocator = gUnifiedMemory;
    } else {
        pAllocator = gCpuOnlyMemory;
    }

    err = Allocator_AddMemoryRegion(pAllocator, pMemDesc);
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
