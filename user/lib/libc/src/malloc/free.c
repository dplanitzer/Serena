//
//  free.c
//  libc
//
//  Created by Dietmar Planitzer on 8/24/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <stdlib.h>
#include "__malloc.h"


void free(void *ptr)
{
    __malloc_lock();
    __lsta_dealloc(__gMainAllocator, ptr);
    __malloc_unlock();
}
