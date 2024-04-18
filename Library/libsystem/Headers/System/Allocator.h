//
//  Allocator.h
//  libsystem
//
//  Created by Dietmar Planitzer on 2/4/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef _SYS_ALLOCATOR_H
#define _SYS_ALLOCATOR_H 1

#include <System/Types.h>

struct Allocator;
typedef struct Allocator* AllocatorRef;

// The allocator that represents the application heap
extern AllocatorRef kAllocator_Main;


extern void* _Nullable Allocator_Allocate(AllocatorRef _Nonnull self, size_t nbytes);
extern void* _Nullable Allocator_Reallocate(AllocatorRef _Nonnull self, void *ptr, size_t new_size);

// Attempts to deallocate the given memory block. Returns EOK on success and
// ENOTBLK if the allocator does not manage the given memory block.
extern void Allocator_Deallocate(AllocatorRef _Nonnull self, void* _Nullable ptr);

// Returns the size of the given memory block. This is the size minus the block
// header and plus whatever additional memory the allocator added based on its
// internal alignment constraints.
extern size_t Allocator_GetBlockSize(AllocatorRef _Nonnull self, void* _Nonnull ptr);

// Returns true if the given pointer is a base pointer of a memory block that
// was allocated with the given allocator.
extern bool Allocator_IsManaging(AllocatorRef _Nonnull self, void* _Nullable ptr);


extern AllocatorRef _Nullable Allocator_Create(void);

#endif /* _SYS_ALLOCATOR_H */
