//
//  Allocator.h
//  Apollo
//
//  Created by Dietmar Planitzer on 2/4/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef Allocator_h
#define Allocator_h

#include "Foundation.h"
#include "Platform.h"


struct _Allocator;
typedef struct _Allocator* AllocatorRef;


// Heap_AllocateBytes options
#define ALLOCATOR_OPTION_CPU        1
#define ALLOCATOR_OPTION_CHIPSET    2
#define ALLOCATOR_OPTION_CLEAR      4


extern AllocatorRef _Nonnull   gMainAllocator;

extern ErrorCode Allocator_Create(const MemoryLayout* _Nonnull pMemLayout, AllocatorRef _Nullable * _Nonnull pOutAllocator);

extern ErrorCode Allocator_AllocateBytes(AllocatorRef _Nonnull pAllocator, Int nbytes, UInt options, Byte* _Nullable * _Nonnull pOutPtr);
extern ErrorCode Allocator_AllocateBytesAt(AllocatorRef _Nonnull pAllocator, Byte* _Nonnull pAddr, Int nbytes);
extern void Allocator_DeallocateBytes(AllocatorRef _Nonnull pAllocator, Byte* _Nullable ptr);

extern void Allocator_Dump(AllocatorRef _Nonnull pAllocator);

#endif /* Allocator_h */
