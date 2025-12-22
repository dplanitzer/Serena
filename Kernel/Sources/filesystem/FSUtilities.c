//
//  FSUtilities.c
//  kernel
//
//  Created by Dietmar Planitzer on 10/17/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "FSUtilities.h"
#include <hal/clock.h>
#include <kern/kalloc.h>
#include <kern/kernlib.h>


// Returns the current time. This time value is suitable for use as a timestamp
// for filesystem objects.
void FSGetCurrentTime(struct timespec* _Nonnull ts)
{
    clock_gettime(g_mono_clock, ts);
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


// Returns true if the argument is a power-of-2 value; false otherwise
bool FSIsPowerOf2(size_t n)
{
    return siz_ispow2(n);
}

// Calculates the power-of-2 value greater equal the given size_t value
size_t FSPowerOf2Ceil(size_t n)
{
    return siz_pow2_ceil(n);
}

// Calculates the log-2 of the given value
unsigned int FSLog2(size_t n)
{
    return siz_log2(n);
}
