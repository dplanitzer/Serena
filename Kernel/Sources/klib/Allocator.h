//
//  Allocator.h
//  Apollo
//
//  Created by Dietmar Planitzer on 2/4/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef Allocator_h
#define Allocator_h

#include <klib/Error.h>
#include "Platform.h"


struct _Allocator;
typedef struct _Allocator* AllocatorRef;


extern ErrorCode Allocator_Create(const MemoryDescriptor* _Nonnull pMemDesc, AllocatorRef _Nullable * _Nonnull pOutAllocator);

extern ErrorCode Allocator_AddMemoryRegion(AllocatorRef _Nonnull pAllocator, const MemoryDescriptor* _Nonnull pMemDesc);
extern Bool Allocator_IsManaging(AllocatorRef _Nonnull pAllocator, void* _Nullable ptr);

extern ErrorCode Allocator_AllocateBytes(AllocatorRef _Nonnull pAllocator, ByteCount nbytes, void* _Nullable * _Nonnull pOutPtr);

// Attempts to deallocate the given memory block. Returns EOK on success and
// ENOTBLK if the allocator does not manage the given memory block.
extern ErrorCode Allocator_DeallocateBytes(AllocatorRef _Nonnull pAllocator, void* _Nullable ptr);

extern void Allocator_Dump(AllocatorRef _Nonnull pAllocator);
extern void Allocator_DumpMemoryRegions(AllocatorRef _Nonnull pAllocator);

#endif /* Allocator_h */
