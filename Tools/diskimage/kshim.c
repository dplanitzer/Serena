//
//  kshim.c
//  diskimage
//
//  Created by Dietmar Planitzer on 3/10/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "diskimage.h"
#include <stdlib.h>

TimeInterval MonotonicClock_GetCurrentTime(void)
{
    TimeInterval ti;

    timespec_get(&ti, TIME_UTC);
    return ti;
}

errno_t kalloc(size_t nbytes, void** pOutPtr)
{
    *pOutPtr = malloc(nbytes);
    return (*pOutPtr) ? EOK : ENOMEM;
}

extern void kfree(void* ptr)
{
    free(ptr);
}
