//
//  malloc.c
//  Apollo
//
//  Created by Dietmar Planitzer on 8/24/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include "Allocator.h"
#include <System.h>


// XXX initialize the malloc package

static AllocatorRef gAllocator;


static errno_t malloc_expand_backingstore_by(size_t nbytes)
{
    void* ptr;
    errno_t err = __alloc_address_space(nbytes, &ptr);
        
    if (err == 0) {
        MemoryDescriptor md;
        md.lower = ptr;
        md.upper = ((char*)ptr) + nbytes;

        err = Allocator_AddMemoryRegion(gAllocator, &md);
    }
    return err;
}

void *malloc(size_t size)
{
    // XXX add locking
    void* ptr = NULL;
    errno_t err = Allocator_AllocateBytes(gAllocator, size, &ptr);

    if (err == ENOMEM) {
        err = malloc_expand_backingstore_by(__min(size, 64 * 1024));
        if (err == 0) {
            err = Allocator_AllocateBytes(gAllocator, size, &ptr);
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
    Allocator_DeallocateBytes(gAllocator, ptr);
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
    const size_t old_size = (ptr) ? Allocator_GetBlockSize(gAllocator, ptr) : 0;
    
    if (old_size == new_size) {
        return ptr;
    }

    void *np = malloc(new_size);

    if (np) {
        memcpy(np, ptr, old_size);
        free(ptr);
    }

    return np;
}
