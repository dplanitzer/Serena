//
//  Platform.h
//  Kernel
//
//  Created by Dietmar Planitzer on 2/4/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef Platform_h
#define Platform_h

#include <kern/types.h>


//
// Memory
//

// Supported max number of memory descriptors
#define MEMORY_DESCRIPTORS_CAPACITY  8

// The type of memory
// Memory accessible to the CPU only
#define MEM_TYPE_MEMORY         0
// Memory accessible to the CPU and I/O (GPU, Audio, etc)
#define MEM_TYPE_UNIFIED_MEMORY 1

// A memory descriptor describes a contiguous range of RAM
typedef struct MemoryDescriptor {
    char* _Nonnull  lower;
    char* _Nonnull  upper;
    int8_t          type;       // MEM_TYPE_XXX
    uint8_t         reserved[3];
} MemoryDescriptor;

typedef struct MemoryLayout {
    int                 descriptor_count;
    MemoryDescriptor    descriptor[MEMORY_DESCRIPTORS_CAPACITY];
} MemoryLayout;

#endif /* Platform_h */
