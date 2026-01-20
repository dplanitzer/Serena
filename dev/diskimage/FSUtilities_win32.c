//
//  FSUtilities_win32.c
//  diskimage
//
//  Created by Dietmar Planitzer on 10/17/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include <stdlib.h>
#include <time.h>
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
