//
//  malloc.c
//  Apollo
//
//  Created by Dietmar Planitzer on 8/24/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <stdlib.h>
#include <string.h>


void *malloc(size_t size)
{
    // XXX implement me
    return NULL;
}

void free(void *ptr)
{
    // XXX implement me
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
    const size_t old_size = 0; // XXX get old size
    
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
