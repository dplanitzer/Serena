//
//  Allocator.h
//  Apollo
//
//  Created by Dietmar Planitzer on 2/4/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef _ALLOCATOR_H
#define _ALLOCATOR_H 1

#include <__stddef.h>


// A memory descriptor describes a contiguous range of RAM
typedef struct _MemoryDescriptor {
    char* _Nonnull  lower;
    char* _Nonnull  upper;
} MemoryDescriptor;


struct _Allocator;
typedef struct _Allocator* AllocatorRef;


extern errno_t Allocator_Create(const MemoryDescriptor* _Nonnull pMemDesc, AllocatorRef _Nullable * _Nonnull pOutAllocator);

extern errno_t Allocator_AddMemoryRegion(AllocatorRef _Nonnull pAllocator, const MemoryDescriptor* _Nonnull pMemDesc);
extern bool Allocator_IsManaging(AllocatorRef _Nonnull pAllocator, void* _Nullable ptr);

extern errno_t Allocator_AllocateBytes(AllocatorRef _Nonnull pAllocator, size_t nbytes, void* _Nullable * _Nonnull pOutPtr);

// Attempts to deallocate the given memory block. Returns EOK on success and
// ENOTBLK if the allocator does not manage the given memory block.
extern errno_t Allocator_DeallocateBytes(AllocatorRef _Nonnull pAllocator, void* _Nullable ptr);

// Returns the size of the given memory block. This is the size minus the block
// header and plus whatever additional memory the allocator added based on its
// internal alignment costraints.
extern size_t Allocator_GetBlockSize(AllocatorRef _Nonnull pAllocator, void* _Nonnull ptr);

extern void Allocator_Dump(AllocatorRef _Nonnull pAllocator);
extern void Allocator_DumpMemoryRegions(AllocatorRef _Nonnull pAllocator);

#endif /* _ALLOCATOR_H */
