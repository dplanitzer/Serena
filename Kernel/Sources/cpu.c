//
//  cpu.c
//  Apollo
//
//  Created by Dietmar Planitzer on 2/10/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "Platform.h"
#include "SystemDescription.h"


// Returns true if the CPU has a 32bit address bus
Bool cpu_is32bit(void)
{
    return SystemDescription_GetShared()->cpu_model >= CPU_MODEL_68020;
}

// Returns the model name for the CPU
const Character* _Nonnull cpu_get_model_name(void)
{
    switch (SystemDescription_GetShared()->cpu_model) {
        case CPU_MODEL_68000:
            return "68000";
            
        case CPU_MODEL_68010:
            return "68010";
            
        case CPU_MODEL_68020:
            return "68020";
            
        case CPU_MODEL_68030:
            return "68030";
            
        case CPU_MODEL_68040:
            return "68040";
            
        case CPU_MODEL_68060:
            return "68060";
            
        default:
            return "??";
    }
}

// Returns the model name for the FPU
const Character* _Nonnull fpu_get_model_name(void)
{
    switch (SystemDescription_GetShared()->fpu_model) {
        case FPU_MODEL_NONE:
            return "none";
            
        case FPU_MODEL_68881:
            return "68881";
            
        case FPU_MODEL_68882:
            return "68882";
            
        case FPU_MODEL_68040:
            return "68040";
            
        case FPU_MODEL_68060:
            return "68060";
            
        default:
            return "??";
    }
}
