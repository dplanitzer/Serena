//
//  __malloc.c
//  libc
//
//  Created by Dietmar Planitzer on 8/24/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <stdlib.h>
#include "__malloc.h"


AllocatorRef __kAllocator_Main;

void __malloc_init(void)
{
    __kAllocator_Main = __Allocator_Create();
}
