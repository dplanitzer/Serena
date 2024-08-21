//
//  __malloc.c
//  libc
//
//  Created by Dietmar Planitzer on 8/24/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <stdlib.h>
#include <System/Lock.h>
#include "__malloc.h"


AllocatorRef    __gMainAllocator;
static Lock     __gMallocLock;


void __malloc_init(void)
{
    __gMainAllocator = __Allocator_Create();
    Lock_Init(&__gMallocLock);
}

void __malloc_lock(void)
{
    Lock_Lock(&__gMallocLock);
}

void __malloc_unlock(void)
{
    Lock_Unlock(&__gMallocLock);
}
