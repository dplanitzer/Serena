//
//  Kalloc.h
//  diskimage
//
//  Created by Dietmar Planitzer on 3/12/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef klib_Kalloc_h
#define klib_Kalloc_h

#include <klib/Types.h>
#include <klib/Error.h>


// kalloc_options options
// Allocate from unified memory (accessible to CPU and the chipset)
#define KALLOC_OPTION_UNIFIED    1
// Clear the allocated memory block
#define KALLOC_OPTION_CLEAR      2


// Allocates memory from the kernel heap. Returns NULL if the memory could not be
// allocated. 'options' is a combination of the HEAP_ALLOC_OPTION_XXX flags.
extern errno_t kalloc_options(ssize_t nbytes, unsigned int options, void* _Nullable * _Nonnull pOutPtr);

// Allocates uninitialized CPU-accessible memory from the kernel heap. Returns
// NULL if the memory could not be allocated. The returned memory is not
// necessarily accessible to I/O DMA operations. Use kalloc_options() with a
// suitable option if DMA accessability is desired.
#define kalloc(__nbytes, __pOutPtr) \
    kalloc_options(__nbytes, 0, __pOutPtr)

// Same as kalloc() but allocated memory that is filled with zeros.
#define kalloc_cleared(__nbytes, __pOutPtr) \
    kalloc_options(__nbytes, KALLOC_OPTION_CLEAR, __pOutPtr)

// Same as kalloc() but allocates unified memory.
#define kalloc_unified(__nbytes, __pOutPtr) \
    kalloc_options(__nbytes, KALLOC_OPTION_UNIFIED, __pOutPtr)

// Frees kernel memory allocated with the kalloc() function.
extern void kfree(void* _Nullable ptr);

#endif /* klib_Kalloc_h */
