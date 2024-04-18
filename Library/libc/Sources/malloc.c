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
    void* ptr = Allocator_Allocate(kAllocator_Main, size);
    
    if (ptr == NULL) {
        errno = ENOMEM;
    }
    return ptr;
}

void free(void *ptr)
{
    Allocator_Deallocate(kAllocator_Main, ptr);
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
    void* np = Allocator_Reallocate(kAllocator_Main, ptr, new_size);

    if (np == NULL) {
        errno = ENOMEM;
    }
    return np;
}
