//
//  Heap.h
//  Apollo
//
//  Created by Dietmar Planitzer on 2/4/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef Heap_h
#define Heap_h

#include "Atomic.h"
#include "Lock.h"
#include "SystemDescription.h"


// A heap block structure describes a freed or allocated block of memory. The
// structure is placed right in front of the memory block. Note that the block
// size includes the header size.
typedef struct _Heap_Block {
    struct _Heap_Block* _Nullable   next;
    UInt                            size;   // Max size of a free block is 4GB; max size of an allocated block is 2GB
} Heap_Block;


// A heap memory descriptor describes a region of contiguous memory which is
// managed by the heap. Each such region has its own private list of free memory
// blocks.
typedef struct _Heap_MemoryDescriptor {
    Byte* _Nonnull          lower;
    Byte* _Nonnull          upper;
    // Every memory region has its own private free list
    Heap_Block* _Nullable   first_free_block;
    UInt8                   accessibility;      // MEM_ACCESS_XXX
    UInt8                   reserved[3];
} Heap_MemoryDescriptor;


// The heap structure. A heap manages memory from a pool of memory contiguous
// regions.
typedef struct _Heap {
    Int                             descriptors_count;
    Heap_MemoryDescriptor* _Nonnull descriptors;
    Heap_Block* _Nullable           first_allocated_block;
    Lock                            lock;
} Heap;


// Heap_AllocateBytes options
#define HEAP_ALLOC_OPTION_CPU       1
#define HEAP_ALLOC_OPTION_CHIPSET   2
#define HEAP_ALLOC_OPTION_CLEAR     4


//
// Convenience memory management functions
//

// Allocates uninitialized CPU-accessible memory from the kernel heap. Returns
// NULL if the memory could not be allocated. The returned memory is not
// necessarily accessibe to I/O DMA operations. Use kalloc_options() with a
// suitable option if DMA accessability is desired.
extern ErrorCode kalloc(Int nbytes, Byte* _Nullable * _Nonnull pOutPtr);

// Same as kalloc() but allocated memory that is filled with zeros.
extern ErrorCode kalloc_cleared(Int nbytes, Byte* _Nullable * _Nonnull pOutPtr);

// Allocates memory from the kernel heap. Returns NULL if the memory could not be
// allocated. 'options' is a combination of the HEAP_ALLOC_OPTION_XXX flags.
extern ErrorCode kalloc_options(Int nbytes, UInt options, Byte* _Nullable * _Nonnull pOutPtr);

// Frees kernel memory allocated with the kalloc() function.
extern void kfree(Byte* _Nullable ptr);


//
// Heap functions
//

// Returns a reference to the shared kernel heap
extern Heap* _Nonnull Heap_GetShared(void);

extern ErrorCode Heap_Create(const MemoryDescriptor* _Nonnull pMemDesc, Int nMemDescs, Heap* _Nullable * _Nonnull pOutHeap);

extern ErrorCode Heap_AllocateBytes(Heap* _Nonnull pHeap, Int nbytes, UInt options, Byte* _Nullable * _Nonnull pOutPtr);
extern ErrorCode Heap_AllocateBytesAt(Heap* _Nonnull pHeap, Byte* _Nonnull pAddr, Int nbytes);
extern void Heap_DeallocateBytes(Heap* _Nonnull pHeap, Byte* _Nullable ptr);

extern void Heap_Dump(Heap* _Nonnull pHeap);

#endif /* Heap_h */
