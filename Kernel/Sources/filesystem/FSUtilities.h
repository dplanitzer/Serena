//
//  FSUtilities.h
//  kernel
//
//  Created by Dietmar Planitzer on 10/17/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef FSUtilities_h
#define FSUtilities_h

#include <klib/Types.h>
#include <System/Error.h>
#include <System/TimeInterval.h>


// This header file defines functions for use in filesystem implementations.


// Returns the current time. This time value is suitable for use as a timestamp
// for filesystem objects.
extern TimeInterval FSGetCurrentTime(void);


// Allocates a memory block. Note that the allocated block is not cleared.
extern errno_t FSAllocate(size_t nbytes, void* _Nullable * _Nonnull pOutPtr);

// Allocates a memory block.
extern errno_t FSAllocateCleared(size_t nbytes, void* _Nullable * _Nonnull pOutPtr);

// Frees a memory block allocated by FSAllocate().
extern void FSDeallocate(void* ptr);

#endif /* FSUtilities_h */
