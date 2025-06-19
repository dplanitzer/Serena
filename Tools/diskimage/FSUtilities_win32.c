//
//  FSUtilities_win32.c
//  diskimage
//
//  Created by Dietmar Planitzer on 10/17/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include <filesystem/FSUtilities.h>
#include <stdlib.h>


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


bool FSIsPowerOf2(size_t n)
{
    return (n && (n & (n - 1)) == 0) ? true : false;
}

size_t FSPowerOf2Ceil(size_t n)
{
    if (n && !(n & (n - 1))) {
        return n;
    } else {
        unsigned long p = 1;
        
        while (p < n) {
            p <<= 1;
        }
        
        return p;
    }
}

unsigned int FSLog2(size_t n)
{
    size_t p = 1;
    unsigned int b = 0;

    while (p < n) {
        p <<= 1;
        b++;
    }
        
    return b;
}
