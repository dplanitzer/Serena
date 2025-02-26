//
//  FSUtilities.c
//  kernel
//
//  Created by Dietmar Planitzer on 10/17/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "FSUtilities.h"
#include <hal/MonotonicClock.h>


// Returns the current time. This time value is suitable for use as a timestamp
// for filesystem objects.
TimeInterval FSGetCurrentTime(void)
{
    return MonotonicClock_GetCurrentTime();
}


// Allocates a memory block. Note that the allocated block is not cleared.
errno_t FSAllocate(size_t nbytes, void* _Nullable * _Nonnull pOutPtr)
{
    return kalloc(nbytes, pOutPtr);
}

// Allocates a memory block.
errno_t FSAllocateCleared(size_t nbytes, void* _Nullable * _Nonnull pOutPtr)
{
    return kalloc_cleared(nbytes,  pOutPtr);
}

// Frees a memory block allocated by FSAllocate().
void FSDeallocate(void* ptr)
{
    kfree(ptr);
}


// Calculates the power-of-2 value greater equal the given size_t value
size_t FSPowerOf2Ceil(size_t n)
{
    return spow2_ceil(n);
}
