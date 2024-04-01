//
//  Allocator.h
//  libc
//
//  Created by Dietmar Planitzer on 2/4/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef _ALLOCATOR_H
#define _ALLOCATOR_H 1

#include <__stddef.h>

// Enable for debugging support
#define ALLOCATOR_DEBUG


// A memory descriptor describes a contiguous range of RAM
typedef struct MemoryDescriptor {
    char* _Nonnull  lower;
    char* _Nonnull  upper;
} MemoryDescriptor;


struct Allocator;
typedef struct Allocator* AllocatorRef;


extern errno_t __Allocator_Create(const MemoryDescriptor* _Nonnull pMemDesc, AllocatorRef _Nullable * _Nonnull pOutAllocator);

extern errno_t __Allocator_AddMemoryRegion(AllocatorRef _Nonnull pAllocator, const MemoryDescriptor* _Nonnull pMemDesc);
extern bool __Allocator_IsManaging(AllocatorRef _Nonnull pAllocator, void* _Nullable ptr);

extern errno_t __Allocator_AllocateBytes(AllocatorRef _Nonnull pAllocator, size_t nbytes, void* _Nullable * _Nonnull pOutPtr);

// Attempts to deallocate the given memory block. Returns EOK on success and
// ENOTBLK if the allocator does not manage the given memory block.
extern errno_t __Allocator_DeallocateBytes(AllocatorRef _Nonnull pAllocator, void* _Nullable ptr);

// Returns the size of the given memory block. This is the size minus the block
// header and plus whatever additional memory the allocator added based on its
// internal alignment constraints.
extern size_t __Allocator_GetBlockSize(AllocatorRef _Nonnull pAllocator, void* _Nonnull ptr);

#ifdef ALLOCATOR_DEBUG
extern void __Allocator_Dump(AllocatorRef _Nonnull pAllocator);
extern void __Allocator_DumpMemoryRegions(AllocatorRef _Nonnull pAllocator);
#endif

#endif /* _ALLOCATOR_H */
