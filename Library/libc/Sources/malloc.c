//
//  malloc.c
//  libc
//
//  Created by Dietmar Planitzer on 8/24/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <stdlib.h>
#include <string.h>
#include <System/Lock.h>
#include <System/Process.h>
#include <__globals.h>
#include <__stddef.h>
#include "Allocator.h"


// Initializes the malloc subsystem and does the initial heap allocation.
// The initial heap size is INITIAL_HEAP_SIZE
#define INITIAL_HEAP_SIZE   __Ceil_PowerOf2(64*1024, CPU_PAGE_SIZE)
#define EXPANSION_HEAP_SIZE __Ceil_PowerOf2(64*1024, CPU_PAGE_SIZE)


static Lock __gLock;

void __malloc_init(void)
{
    MemoryDescriptor md;
    void* ptr;

    try_bang(Lock_Init(&__gLock));
    try_bang(Process_AllocateAddressSpace(INITIAL_HEAP_SIZE, &ptr));
    md.lower = ptr;
    md.upper = ((char*)ptr) + INITIAL_HEAP_SIZE;
    try_bang(__Allocator_Create(&md, &__gAllocator));
}

static errno_t __malloc_expand_backingstore_by(size_t nbytes)
{
    decl_try_err();
    void* ptr;
    
    err = Process_AllocateAddressSpace(nbytes, &ptr);    
    if (err == EOK) {
        MemoryDescriptor md;
        md.lower = ptr;
        md.upper = ((char*)ptr) + nbytes;

        err = __Allocator_AddMemoryRegion(__gAllocator, &md);
    }

    return err;
}

static void * __malloc(size_t size)
{
    decl_try_err();
    void* ptr = NULL;
    
    err = __Allocator_AllocateBytes(__gAllocator, size, &ptr);
    if (err == ENOMEM) {
        const size_t ceiledSize = __Ceil_PowerOf2(size, CPU_PAGE_SIZE);
        const size_t minExpansionSize = EXPANSION_HEAP_SIZE;

        err = __malloc_expand_backingstore_by(__min(ceiledSize, minExpansionSize));
        if (err == EOK) {
            err = __Allocator_AllocateBytes(__gAllocator, size, &ptr);
        }
    }

    if (err != EOK) {
        errno = err;
    }

    return ptr;
}

static void __free(void *ptr)
{
    __Allocator_DeallocateBytes(__gAllocator, ptr);
}



void *malloc(size_t size)
{
    try_bang(Lock_Lock(&__gLock));
    void* ptr = __malloc(size);
    try_bang(Lock_Unlock(&__gLock));

    return ptr;
}

void free(void *ptr)
{
    try_bang(Lock_Lock(&__gLock));
    __free(ptr);
    try_bang(Lock_Unlock(&__gLock));
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
    try_bang(Lock_Lock(&__gLock));
    const size_t old_size = (ptr) ? __Allocator_GetBlockSize(__gAllocator, ptr) : 0;
    void* np;
    
    if (old_size != new_size) {
        np = __malloc(new_size);

        if (np) {
            //XXX want to do the copy outside the lock one day
            memcpy(np, ptr, __min(old_size, new_size));
            __free(ptr);
        }
    }
    else {
        np = ptr;
    }
    try_bang(Lock_Unlock(&__gLock));

    return np;
}

void malloc_dump(void)
{
#ifdef ALLOCATOR_DEBUG
    Lock_Lock(&__gLock);
    __Allocator_Dump(__gAllocator);
    Lock_Unlock(&__gLock);
#endif
}