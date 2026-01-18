//
//  malloc.c
//  libc
//
//  Created by Dietmar Planitzer on 8/24/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <stdlib.h>
#include "__malloc.h"


void *malloc(size_t size)
{
    __malloc_lock();
    void* ptr = __lsta_alloc(__gMainAllocator, size);
    
    if (ptr == NULL) {
        __malloc_nomem();
    }
    __malloc_unlock();

    return ptr;
}
