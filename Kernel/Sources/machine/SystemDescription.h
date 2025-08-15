//
//  SystemDescription.h
//  kernel
//
//  Created by Dietmar Planitzer on 2/4/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef SystemDescription_h
#define SystemDescription_h

#include <kern/types.h>

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


// The system description
// Note: Keep in sync with machine/hal/lowmem.i
typedef struct SystemDescription {
    int8_t          cpu_model;
    int8_t          fpu_model;

    uint8_t         chipset_version;
    uint8_t         chipset_ramsey_version;     // RAMSEY custom chip version. 0 means no RAMSEY and thus a 16bit Amiga (A500 / A2000)
    char*           chipset_upper_dma_limit;    // Chipset DMA is limited to addresses below this address
    
    // These are memory regions that are accessible to the CPU without having to
    // auto configure the expansion bus. 
    MemoryLayout    motherboard_ram;    
} SystemDescription;


// Returns a reference to the shared system description.
extern SystemDescription* _Nonnull gSystemDescription;

// Returns the amount of physical RAM in the machine.
extern size_t SystemDescription_GetRamSize(const SystemDescription* _Nonnull self);

#endif /* SystemDescription_h */
