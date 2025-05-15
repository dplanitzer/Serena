//
//  FSUtilities.h
//  kernel
//
//  Created by Dietmar Planitzer on 10/17/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef FSUtilities_h
#define FSUtilities_h

#include <kern/errno.h>
#include <kern/types.h>


// This header file defines functions for use in filesystem implementations.


// Returns the current time. This time value is suitable for use as a timestamp
// for filesystem objects.
extern struct timespec FSGetCurrentTime(void);


// Allocates a memory block. Note that the allocated block is not cleared.
extern errno_t FSAllocate(size_t nbytes, void* _Nullable * _Nonnull pOutPtr);

// Allocates a memory block.
extern errno_t FSAllocateCleared(size_t nbytes, void* _Nullable * _Nonnull pOutPtr);

// Frees a memory block allocated by FSAllocate().
extern void FSDeallocate(void* ptr);


// Returns true if the argument is a power-of-2 value; false otherwise
extern bool FSIsPowerOf2(size_t n);

// Calculates the power-of-2 value greater equal the given size_t value
extern size_t FSPowerOf2Ceil(size_t n);

// Calculates the log-2 of the given value
extern unsigned int FSLog2(size_t n);

#endif /* FSUtilities_h */
