//
//  sys_desc_t.h
//  kernel
//
//  Created by Dietmar Planitzer on 2/4/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef _SYS_DESC_H
#define _SYS_DESC_H

#include <stdint.h>
#include <stddef.h>


// Supported max number of memory descriptors
#define MEM_DESC_CAPACITY  8

// The type of memory
// Memory accessible to the CPU only
#define MEM_TYPE_MEMORY         0
// Memory accessible to the CPU and I/O (GPU, Audio, etc)
#define MEM_TYPE_UNIFIED_MEMORY 1


// A memory descriptor describes a contiguous range of RAM
typedef struct mem_desc {
    char* _Nonnull  lower;
    char* _Nonnull  upper;
    int8_t          type;       // MEM_TYPE_XXX
    uint8_t         reserved[3];
} mem_desc_t;

typedef struct mem_layout {
    int         desc_count;
    mem_desc_t  desc[MEM_DESC_CAPACITY];
} mem_layout_t;


// The system description
// Note: Keep in sync with machine/hw/m68k/lowmem.i
typedef struct sys_desc {
    int8_t          cpu_model;
    int8_t          fpu_model;

    uint8_t         chipset_version;
    uint8_t         chipset_ramsey_version;     // RAMSEY custom chip version. 0 means no RAMSEY and thus a 16bit Amiga (A500 / A2000)
    char*           chipset_upper_dma_limit;    // Chipset DMA is limited to addresses below this address
    
    // These are memory regions that are accessible to the CPU without having to
    // auto configure the expansion bus. 
    mem_layout_t    motherboard_ram;    
} sys_desc_t;


// Returns a reference to the shared system description.
extern sys_desc_t* _Nonnull g_sys_desc;

// Returns the amount of physical RAM in the machine.
extern size_t sys_desc_getramsize(const sys_desc_t* _Nonnull self);

#endif /* _SYS_DESC_H */
