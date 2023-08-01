//
//  SystemDescription.h
//  Apollo
//
//  Created by Dietmar Planitzer on 2/4/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef SystemDescription_h
#define SystemDescription_h

#include "Platform.h"

// The system description
// Note: Keep in sync with lowmem.i
typedef struct _SystemDescription {
    Int8                cpu_model;
    Int8                fpu_model;
    // All fields above this point are filled in before OnReset() is called.
    // All fields below this point are initialized by the SystemDescription_Init() function
    UInt8               chipset_version;
    UInt8               chipset_ramsey_version;     // RAMSEY custom chip version. 0 means no RAMSEY and thus a 16bit Amiga (A500 / A2000)
    
    Int32               quantum_duration_ns;        // Quantum duration in terms of nenoseconds
    Int16               quantum_duration_cycles;    // Quantum duration in terms of timer cycles
    Int16               ns_per_quantum_timer_cycle; // Lenght of a quantu timer cycle in nanoseconds
    
    MemoryLayout        memory;
    ExpansionBus        expansion;
} SystemDescription;


// Returns a reference to the shared system description.
extern SystemDescription* _Nonnull gSystemDescription;

#endif /* SystemDescription_h */
