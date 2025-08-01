//
//  SystemDescription.h
//  kernel
//
//  Created by Dietmar Planitzer on 2/4/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef SystemDescription_h
#define SystemDescription_h

#include <machine/Platform.h>

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
