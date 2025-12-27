//
//  FSUtilities_win32.c
//  diskimage
//
//  Created by Dietmar Planitzer on 10/17/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include <stdlib.h>
#include <ext/math.h>
#include <filesystem/FSUtilities.h>


// Returns the current time. This time value is suitable for use as a timestamp
// for filesystem objects.
void FSGetCurrentTime(struct timespec* _Nonnull ts)
{
    timespec_get(ts, TIME_UTC);
}


// Allocates a memory block. Note that the allocated block is not cleared.
errno_t FSAllocate(size_t nbytes, void* _Nullable * _Nonnull pOutPtr)
{
    *pOutPtr = malloc(nbytes);
    return *pOutPtr ? EOK : ENOMEM;
}

// Allocates a memory block.
errno_t FSAllocateCleared(size_t nbytes, void* _Nullable * _Nonnull pOutPtr)
{
    *pOutPtr = calloc(1, nbytes);
    return *pOutPtr ? EOK : ENOMEM;
}

// Frees a memory block allocated by FSAllocate().
void FSDeallocate(void* ptr)
{
    free(ptr);
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
