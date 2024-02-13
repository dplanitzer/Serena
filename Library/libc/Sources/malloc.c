//
//  malloc.c
//  Apollo
//
//  Created by Dietmar Planitzer on 8/24/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <stdlib.h>
#include <string.h>
#include <System/Process.h>
#include <__globals.h>
#include <__stddef.h>
#include "Allocator.h"


// Initializes the malloc subsystem and does the initial heap allocation.
// The initial heap size is INITIAL_HEAP_SIZE
#define INITIAL_HEAP_SIZE   __Ceil_PowerOf2(64*1024, CPU_PAGE_SIZE)
#define EXPANSION_HEAP_SIZE __Ceil_PowerOf2(64*1024, CPU_PAGE_SIZE)

void __malloc_init(void)
{
    MemoryDescriptor md;
    void* ptr;

    try_bang(Process_AllocateAddressSpace(INITIAL_HEAP_SIZE, &ptr));
    md.lower = ptr;
    md.upper = ((char*)ptr) + INITIAL_HEAP_SIZE;
    try_bang(__Allocator_Create(&md, &__gAllocator));
}

static errno_t __malloc_expand_backingstore_by(size_t nbytes)
{
    void* ptr;
    errno_t err = Process_AllocateAddressSpace(nbytes, &ptr);
        
    if (err == 0) {
        MemoryDescriptor md;
        md.lower = ptr;
        md.upper = ((char*)ptr) + nbytes;

        err = __Allocator_AddMemoryRegion(__gAllocator, &md);
    }
    return err;
}

void *malloc(size_t size)
{
    // XXX add locking
    void* ptr = NULL;
    errno_t err = __Allocator_AllocateBytes(__gAllocator, size, &ptr);

    if (err == ENOMEM) {
        const size_t ceiledSize = __Ceil_PowerOf2(size, CPU_PAGE_SIZE);
        const size_t minExpansionSize = EXPANSION_HEAP_SIZE;

        err = __malloc_expand_backingstore_by(__min(ceiledSize, minExpansionSize));
        if (err == 0) {
            err = __Allocator_AllocateBytes(__gAllocator, size, &ptr);
        }
    }

    if (err != 0) {
        errno = err;
    }

    return ptr;
}

void free(void *ptr)
{
    // XXX add locking
    __Allocator_DeallocateBytes(__gAllocator, ptr);
}

void *calloc(size_t num, size_t size)
{
    const size_t len = num * size;
    void *p = malloc(len);

    if (p) {
        memset(p, 0, len);
    }
    return p;
}

void *realloc(void *ptr, size_t new_size)
{
    const size_t old_size = (ptr) ? __Allocator_GetBlockSize(__gAllocator, ptr) : 0;
    
    if (old_size == new_size) {
        return ptr;
    }

    void *np = malloc(new_size);

    if (np) {
        memcpy(np, ptr, __min(old_size, new_size));
        free(ptr);
    }

    return np;
}

void malloc_dump(void)
{
#ifdef ALLOCATOR_DEBUG
    __Allocator_Dump(__gAllocator);
#endif
}