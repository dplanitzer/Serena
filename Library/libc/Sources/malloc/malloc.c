//
//  malloc.c
//  libc
//
//  Created by Dietmar Planitzer on 8/24/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <stdlib.h>
#include <errno.h>
#include "__malloc.h"


void *malloc(size_t size)
{
    __malloc_lock();
    void* ptr = __Allocator_Allocate(__gMainAllocator, size);
    
    if (ptr == NULL) {
        errno = ENOMEM;
    }
    __malloc_unlock();

    return ptr;
}
