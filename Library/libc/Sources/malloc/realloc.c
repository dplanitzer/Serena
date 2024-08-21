//
//  realloc.c
//  libc
//
//  Created by Dietmar Planitzer on 8/24/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <stdlib.h>
#include <errno.h>
#include "__malloc.h"


void *realloc(void *ptr, size_t new_size)
{
    __malloc_lock();
    void* np = __Allocator_Reallocate(__gMainAllocator, ptr, new_size);

    if (np == NULL) {
        errno = ENOMEM;
    }
    __malloc_unlock();
    
    return np;
}
