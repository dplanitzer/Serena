//
//  Allocator.h
//  libc
//
//  Created by Dietmar Planitzer on 2/4/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef _ALLOCATOR_H
#define _ALLOCATOR_H 1

#include <ext/errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct Allocator;
typedef struct Allocator* AllocatorRef;

// A memory descriptor describes a contiguous range of RAM that should be managed
// by the allocator.
typedef struct mem_desc_t {
    char* _Nonnull  lower;
    char* _Nonnull  upper;
} mem_desc_t;


// Callback that is invoked by the allocator if it needs more backing store.
// Should return true on success and false on failure. Failure will result in a
// ENOMEM error.
typedef bool (*AllocatorGrowFunc)(AllocatorRef _Nonnull allocator, size_t minByteCount);


extern AllocatorRef _Nullable __Allocator_Create(const mem_desc_t* _Nonnull md, AllocatorGrowFunc _Nullable growFunc);

// Adds the given memory region to the allocator's available memory pool.
extern errno_t __Allocator_AddMemoryRegion(AllocatorRef _Nonnull self, const mem_desc_t* _Nonnull md);

extern void* _Nullable __Allocator_Allocate(AllocatorRef _Nonnull self, size_t nbytes);
extern void* _Nullable __Allocator_Reallocate(AllocatorRef _Nonnull self, void *ptr, size_t new_size);

// Attempts to deallocate the given memory block. Returns EOK on success and
// ENOTBLK if the allocator does not manage the given memory block.
extern errno_t __Allocator_Deallocate(AllocatorRef _Nonnull self, void* _Nullable ptr);

// Returns the size of the given memory block. This is the size minus the block
// header and plus whatever additional memory the allocator added based on its
// internal alignment constraints.
extern errno_t __Allocator_GetBlockSize(AllocatorRef _Nonnull self, void* _Nonnull ptr, size_t* _Nonnull pOutSize);

// Returns true if the given pointer is a base pointer of a memory block that
// was allocated with the given allocator.
extern bool __Allocator_IsManaging(AllocatorRef _Nonnull self, void* _Nullable ptr);

#endif /* _ALLOCATOR_H */
