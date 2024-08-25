//
//  Allocator.h
//  libc
//
//  Created by Dietmar Planitzer on 2/4/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef _ALLOCATOR_H
#define _ALLOCATOR_H 1

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <System/Error.h>

struct Allocator;


#if __LP64__
#define HEAP_ALIGNMENT  16
#elif __ILP32__
#define HEAP_ALIGNMENT  8
#else
#error "don't know how to align heap blocks"
#endif


// Callback that is invoked by the allocator if it needs more backing store
typedef errno_t (*AllocatorGrowFunc)(struct Allocator* _Nonnull pAllocator, size_t minByteCount);


// A memory descriptor describes a contiguous range of RAM that should be managed
// by the allocator.
typedef struct MemoryDescriptor {
    char* _Nonnull  lower;
    char* _Nonnull  upper;
} MemoryDescriptor;


// A memory block structure describes a freed or allocated block of memory. The
// structure is placed right in front of the memory block. Note that the block
// size includes the header size.
typedef struct MemBlock {
    struct MemBlock* _Nullable  next;
    size_t                      size;   // The size includes sizeof(MemBlock).
} MemBlock;


// A heap memory region is a region of contiguous memory which is managed by the
// heap. Each such region has its own private list of free memory blocks.
typedef struct MemRegion {
    struct MemRegion* _Nullable next;
    char* _Nonnull              lower;
    char* _Nonnull              upper;
    MemBlock* _Nullable         first_free_block;   // Every memory region has its own private free list. Note that the blocks on this list are ordered by increasing base address
} MemRegion;


// An allocator manages memory from a pool of memory contiguous regions.
typedef struct Allocator {
    MemRegion* _Nonnull         first_region;
    MemRegion* _Nonnull         last_region;
    MemBlock* _Nullable         first_allocated_block;  // Unordered list of allocated blocks (no matter from which memory region they were allocated)
    AllocatorGrowFunc _Nullable grow_func;
} Allocator;

typedef struct Allocator* AllocatorRef;


extern AllocatorRef _Nullable __Allocator_Create(const MemoryDescriptor* _Nonnull md, AllocatorGrowFunc _Nullable growFunc);

// Adds the given memory region to the allocator's available memory pool.
extern errno_t __Allocator_AddMemoryRegion(AllocatorRef _Nonnull pAllocator, const MemoryDescriptor* _Nonnull md);

extern void* _Nullable __Allocator_Allocate(AllocatorRef _Nonnull self, size_t nbytes);
extern void* _Nullable __Allocator_Reallocate(AllocatorRef _Nonnull self, void *ptr, size_t new_size);

// Attempts to deallocate the given memory block. Returns EOK on success and
// ENOTBLK if the allocator does not manage the given memory block.
extern errno_t __Allocator_Deallocate(AllocatorRef _Nonnull self, void* _Nullable ptr);

// Returns the size of the given memory block. This is the size minus the block
// header and plus whatever additional memory the allocator added based on its
// internal alignment constraints.
extern errno_t __Allocator_GetBlockSize(AllocatorRef _Nonnull pAllocator, void* _Nonnull ptr, size_t* _Nonnull pOutSize);

// Returns true if the given pointer is a base pointer of a memory block that
// was allocated with the given allocator.
extern bool __Allocator_IsManaging(AllocatorRef _Nonnull self, void* _Nullable ptr);

#endif /* _ALLOCATOR_H */
