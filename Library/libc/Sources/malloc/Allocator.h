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
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <System/Error.h>

#if defined(__ILP32__)
typedef int32_t word_t;
#define WORD_SIZE       4
#define WORD_MAX    INT_MAX
// 'bhdr'
#define HEADER_PATTERN  ((word_t)0x62686472)
// 'btrl'
#define TRAILER_PATTERN  ((word_t)0x6274726c)
#elif defined(__LLP64__) || defined(__LP64__)
typedef int64_t word_t;
#define WORD_SIZE       8
#define WORD_MAX    LLONG_MAX
// 'bhdr'
#define HEADER_PATTERN  ((word_t)0x6268647272646862)
// 'btrl'
#define TRAILER_PATTERN  ((word_t)0x6274726c6c727462)
#else
#error "unknown data model"
#endif

#define MIN_GROSS_BLOCK_SIZE    (sizeof(block_header_t) + sizeof(word_t) + sizeof(block_trailer_t))
#define MAX_NET_BLOCK_SIZE      (WORD_MAX - sizeof(block_header_t) - sizeof(block_trailer_t))


struct Allocator;

// A memory block (freed or allocated) has a header at the beginning (lowest address)
// and a trailer (highest address) at its end. The header and trailer store the
// block size. The size is the gross block size in terms of bytes. So it includes
// the size of the header and the trailer. The sign bit of the block size indicates
// whether the block is in allocated or freed state: sign bit set to 1 means
// allocated and sign bit set to 0 means freed.

typedef struct block_header {
    word_t  size;       // < 0 -> allocated block; > 0 -> free block; == 0 -> invalid; gross block size in bytes := |size|
    word_t  pat;        // HEADER_PATTERN
} block_header_t;

typedef struct block_trailer {
    word_t  pat;        // TRAILER_PATTERN
    word_t  size;       // < 0 -> allocated block; > 0 -> free block; == 0 -> invalid; gross block size in bytes := |size|
} block_trailer_t;


// Callback that is invoked by the allocator if it needs more backing store
typedef errno_t (*AllocatorGrowFunc)(struct Allocator* _Nonnull allocator, size_t minByteCount);


// A memory descriptor describes a contiguous range of RAM that should be managed
// by the allocator.
typedef struct MemoryDescriptor {
    char* _Nonnull  lower;
    char* _Nonnull  upper;
} MemoryDescriptor;


// A memory region manages a contiguous range of memory.
typedef struct mem_region {
    struct mem_region* _Nullable    next;
    char* _Nonnull                  lower;  // Lowest address from which to allocate (word aligned)
    char* _Nonnull                  upper;  // Address just beyond the last allocatable address (word aligned)
} mem_region_t;


// An allocator manages memory from a pool of memory regions.
typedef struct Allocator {
    mem_region_t* _Nonnull      first_region;
    mem_region_t* _Nonnull      last_region;
    AllocatorGrowFunc _Nullable grow_func;
} Allocator;

typedef struct Allocator* AllocatorRef;


extern AllocatorRef _Nullable __Allocator_Create(const MemoryDescriptor* _Nonnull md, AllocatorGrowFunc _Nullable growFunc);

// Adds the given memory region to the allocator's available memory pool.
extern errno_t __Allocator_AddMemoryRegion(AllocatorRef _Nonnull self, const MemoryDescriptor* _Nonnull md);

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
