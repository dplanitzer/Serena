//
//  Kalloc.c
//  kernel
//
//  Created by Dietmar Planitzer on 8/6/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Allocator.h"
#include <machine/sys_desc.h>
#include <kern/assert.h>
#include <kern/kalloc.h>
#include <kern/kernlib.h>
#include <kern/string.h>
#include <sched/mtx.h>


static mtx_t        gLock;
static AllocatorRef gUnifiedMemory;       // CPU + Chipset access (memory range [0..<chipset_upper_dma_limit]) (Required)
static AllocatorRef gCpuOnlyMemory;       // CPU only access      (memory range [chipset_upper_dma_limit...]) (Optional - created on demand if no Fast memory exists in the machine and we later pick up a RAM expansion board)


static mem_desc_t adjusted_memory_descriptor(const mem_desc_t* pMemDesc, char* _Nonnull pInitialHeapBottom, char* _Nonnull pInitialHeapTop)
{
    mem_desc_t md;

    md.lower = __max(pMemDesc->lower, pInitialHeapBottom);
    md.upper = __min(pMemDesc->upper, pInitialHeapTop);
    md.type = pMemDesc->type;

    return md;
}

static errno_t create_allocator(mem_layout_t* _Nonnull pMemLayout, char* _Nonnull pInitialHeapBottom, char* _Nonnull pInitialHeapTop, int8_t memoryType, bool isOptional, AllocatorRef _Nullable * _Nonnull pOutAllocator)
{
    decl_try_err();
    int i = 0;
    AllocatorRef pAllocator = NULL;
    mem_desc_t adjusted_md;

    // Skip over memory regions that are below the kernel heap bottom
    while (i < pMemLayout->desc_count && 
        (pMemLayout->desc[i].upper < pInitialHeapBottom || (pMemLayout->desc[i].upper >= pInitialHeapBottom && pMemLayout->desc[i].type != memoryType))) {
        i++;
    }
    if (i == pMemLayout->desc_count) {
        throw((isOptional) ? EOK : ENOMEM);
    }


    // First valid memory descriptor. Create the allocator based on that. We'll
    // get an ENOMEM error if this memory region isn't big enough
    adjusted_md = adjusted_memory_descriptor(&pMemLayout->desc[i], pInitialHeapBottom, pInitialHeapTop);
    try_null(pAllocator, __Allocator_Create(&adjusted_md, NULL), ENOMEM);


    // Pick up all other memory regions that are at least partially below the
    // kernel heap top
    i++;
    while (i < pMemLayout->desc_count && pMemLayout->desc[i].lower < pInitialHeapTop) {
        if (pMemLayout->desc[i].type == memoryType) {
            adjusted_md = adjusted_memory_descriptor(&pMemLayout->desc[i], pInitialHeapBottom, pInitialHeapTop);
            try(__Allocator_AddMemoryRegion(pAllocator, &adjusted_md));
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
errno_t kalloc_init(const sys_desc_t* _Nonnull pSysDesc, void* _Nonnull pInitialHeapBottom, void* _Nonnull pInitialHeapTop)
{
    decl_try_err();

    mtx_init(&gLock);
    try(create_allocator(&pSysDesc->motherboard_ram, pInitialHeapBottom, pInitialHeapTop, MEM_TYPE_UNIFIED_MEMORY, false, &gUnifiedMemory));
    try(create_allocator(&pSysDesc->motherboard_ram, pInitialHeapBottom, pInitialHeapTop, MEM_TYPE_MEMORY, true, &gCpuOnlyMemory));
    return EOK;

catch:
    return err;
}

// Allocates memory from the kernel heap. Returns NULL if the memory could not be
// allocated. 'options' is a combination of the HEAP_ALLOC_OPTION_XXX flags.
errno_t kalloc_options(size_t nbytes, unsigned int options, void* _Nullable * _Nonnull pOutPtr)
{
    decl_try_err();
    void* ptr = NULL;

    mtx_lock(&gLock);
    if ((options & KALLOC_OPTION_UNIFIED) != 0 || gCpuOnlyMemory == NULL) {
        ptr = __Allocator_Allocate(gUnifiedMemory, nbytes);
    } else {
        ptr = __Allocator_Allocate(gCpuOnlyMemory, nbytes);
        if (ptr == NULL) {
            ptr = __Allocator_Allocate(gUnifiedMemory, nbytes);
        }
    }
    mtx_unlock(&gLock);

    // Zero the memory if requested
    if (ptr && (options & KALLOC_OPTION_CLEAR) != 0) {
        memset(ptr, 0, nbytes);
    }

    *pOutPtr = ptr;
    return (ptr) ? EOK : ENOMEM;
}

// Frees kernel memory allocated with the kalloc() function.
void kfree(void* _Nullable ptr)
{
    decl_try_err();

    mtx_lock(&gLock);
    err = __Allocator_Deallocate(gUnifiedMemory, ptr);

    if (err == ENOTBLK && gCpuOnlyMemory) {
        try_bang(__Allocator_Deallocate(gCpuOnlyMemory, ptr));
    } else if (err != EOK) {
        abort();
    }
    mtx_unlock(&gLock);
}

// Returns the gross size of the given memory block. The gross size may be a bit
// bigger than what was originally requested, because of alignment constraints.
size_t ksize(void* _Nullable ptr)
{
    decl_try_err();
    size_t nbytes = 0;

    mtx_lock(&gLock);
    err = __Allocator_GetBlockSize(gUnifiedMemory, ptr, &nbytes);

    if (err == ENOTBLK && gCpuOnlyMemory) {
        err = __Allocator_GetBlockSize(gCpuOnlyMemory, ptr, &nbytes);
    }

    mtx_unlock(&gLock);

    return nbytes;
}

// Adds the given memory region as a CPU-only access memory region to the kalloc
// heap.
errno_t kalloc_add_memory_region(const mem_desc_t* _Nonnull pMemDesc)
{
    decl_try_err();
    AllocatorRef pAllocator;

    mtx_lock(&gLock);
    if (pMemDesc->upper < g_sys_desc->chipset_upper_dma_limit) {
        err = __Allocator_AddMemoryRegion(gUnifiedMemory, pMemDesc);
    }
    else if (gCpuOnlyMemory) {
        err = __Allocator_AddMemoryRegion(gCpuOnlyMemory, pMemDesc);
    }
    else {
        gCpuOnlyMemory = __Allocator_Create(pMemDesc, NULL);
        if (gCpuOnlyMemory == NULL) {
            err = ENOMEM;
        }
    }
    mtx_unlock(&gLock);

    return err;
}
