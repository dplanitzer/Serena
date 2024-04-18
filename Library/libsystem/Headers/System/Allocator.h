//
//  Allocator.h
//  libsystem
//
//  Created by Dietmar Planitzer on 2/4/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef _SYS_ALLOCATOR_H
#define _SYS_ALLOCATOR_H 1

#include <System/_cmndef.h>
#include <System/Error.h>
#include <System/Types.h>

struct Allocator;
typedef struct Allocator* AllocatorRef;

// The allocator that represents the application heap
extern AllocatorRef kAllocator_Main;


extern errno_t Allocator_AllocateBytes(AllocatorRef _Nonnull pAllocator, size_t nbytes, void* _Nullable * _Nonnull pOutPtr);
extern errno_t Allocator_ReallocateBytes(AllocatorRef _Nonnull pAllocator, void *ptr, size_t new_size, void* _Nullable * _Nonnull pOutPtr);

// Attempts to deallocate the given memory block. Returns EOK on success and
// ENOTBLK if the allocator does not manage the given memory block.
extern errno_t Allocator_DeallocateBytes(AllocatorRef _Nonnull pAllocator, void* _Nullable ptr);

// Returns the size of the given memory block. This is the size minus the block
// header and plus whatever additional memory the allocator added based on its
// internal alignment constraints.
extern size_t Allocator_GetBlockSize(AllocatorRef _Nonnull pAllocator, void* _Nonnull ptr);

// Returns true if the given pointer is a base pointer of a memory block that
// was allocated with the given allocator.
extern bool Allocator_IsManaging(AllocatorRef _Nonnull pAllocator, void* _Nullable ptr);


extern errno_t Allocator_Create(AllocatorRef _Nullable * _Nonnull pOutAllocator);

#endif /* _SYS_ALLOCATOR_H */
