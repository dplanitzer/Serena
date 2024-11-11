//
//  SystemDescription.h
//  kernel
//
//  Created by Dietmar Planitzer on 2/4/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef SystemDescription_h
#define SystemDescription_h

#include "Platform.h"

// The system description
// Note: Keep in sync with lowmem.i
typedef struct SystemDescription {
    int8_t          cpu_model;
    int8_t          fpu_model;

    uint8_t         chipset_version;
    uint8_t         chipset_ramsey_version;     // RAMSEY custom chip version. 0 means no RAMSEY and thus a 16bit Amiga (A500 / A2000)
    char*           chipset_upper_dma_limit;    // Chipset DMA is limited to addresses below this address
    
    int32_t         quantum_duration_ns;        // Quantum duration in terms of nanoseconds
    int16_t         quantum_duration_cycles;    // Quantum duration in terms of timer cycles
    int16_t         ns_per_quantum_timer_cycle; // Length of a quantum timer cycle in nanoseconds
    
    // These are memory regions that are accessible to the CPU without having to
    // auto configure the expansion bus. 
    MemoryLayout    motherboard_ram;    
} SystemDescription;


// Returns a reference to the shared system description.
extern SystemDescription* _Nonnull gSystemDescription;

// Returns the amount of physical RAM in the machine.
extern size_t SystemDescription_GetRamSize(const SystemDescription* _Nonnull self);

#endif /* SystemDescription_h */
