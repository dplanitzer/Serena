//
//  malloc.c
//  libc
//
//  Created by Dietmar Planitzer on 8/24/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <System/Allocator.h>


void *malloc(size_t size)
{
    decl_try_err();
    void* ptr;

    err = Allocator_AllocateBytes(kAllocator_Main, size, &ptr);
    if (err != EOK) {
        errno = err;
        return NULL;
    }
    else {
        return ptr;
    }
}

void free(void *ptr)
{
    Allocator_DeallocateBytes(kAllocator_Main, ptr);
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
    decl_try_err();
    void* np;

    err = Allocator_ReallocateBytes(kAllocator_Main, ptr, new_size, &np);
    if (err != EOK) {
        errno = err;
        return NULL;
    }
    else {
        return np;
    }
}
