//
//  Kalloc.h
//  Apollo
//
//  Created by Dietmar Planitzer on 8/6/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef kalloc_h
#define kalloc_h

#include <klib/Types.h>
#include <klib/Error.h>
#include <SystemDescription.h>


// kalloc_options options
// Allocate from unified memory (accessible to CPU and the chipset)
#define KALLOC_OPTION_UNIFIED    1
// Clear the allocated memory block
#define KALLOC_OPTION_CLEAR      2


// Allocates memory from the kernel heap. Returns NULL if the memory could not be
// allocated. 'options' is a combination of the HEAP_ALLOC_OPTION_XXX flags.
extern ErrorCode kalloc_options(ByteCount nbytes, UInt options, Byte* _Nullable * _Nonnull pOutPtr);

// Allocates uninitialized CPU-accessible memory from the kernel heap. Returns
// NULL if the memory could not be allocated. The returned memory is not
// necessarily accessibe to I/O DMA operations. Use kalloc_options() with a
// suitable option if DMA accessability is desired.
static inline ErrorCode kalloc(ByteCount nbytes, Byte* _Nullable * _Nonnull pOutPtr)
{
    return kalloc_options(nbytes, 0, pOutPtr);
}

// Same as kalloc() but allocated memory that is filled with zeros.
static inline ErrorCode kalloc_cleared(ByteCount nbytes, Byte* _Nullable * _Nonnull pOutPtr)
{
    return kalloc_options(nbytes, KALLOC_OPTION_CLEAR, pOutPtr);
}

// Same as kalloc() but allocates unified memory.
static inline ErrorCode kalloc_unified(ByteCount nbytes, Byte* _Nullable * _Nonnull pOutPtr)
{
    return kalloc_options(nbytes, KALLOC_OPTION_UNIFIED, pOutPtr);
}

// Frees kernel memory allocated with the kalloc() function.
extern void kfree(Byte* _Nullable ptr);

// Adds the given memory region as a CPU-only access memory region to the kalloc
// heap.
extern ErrorCode kalloc_add_memory_region(const MemoryDescriptor* _Nonnull pMemDesc);

// Dumps a description of the kalloc heap to the console
extern void kalloc_dump(void);

// Initializes the kalloc heap.
extern ErrorCode kalloc_init(const SystemDescription* _Nonnull pSysDesc, Byte* _Nonnull pInitialHeapBottom, Byte* _Nonnull pInitialHeapTop);

#endif /* kalloc_h */
