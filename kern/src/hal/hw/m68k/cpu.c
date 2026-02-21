//
//  machine/hw/m68k/cpu.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/9/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <hal/cpu.h>
#include <hal/sys_desc.h>


// Returns the model name of the CPU
const char* _Nonnull cpu_get_model_name(int8_t cpu_model)
{
    switch (cpu_model) {
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

// Returns the model name of the FPU
const char* _Nonnull fpu_get_model_name(int8_t fpu_model)
{
    switch (fpu_model) {
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

bool cpu_inject_sigurgent(excpt_frame_t* _Nonnull efp)
{
    struct sigurgent_frame {
        void* ret_addr;
    };

    extern void sigurgent(void);
    extern void sigurgent_end(void);
    const uintptr_t upc = excpt_frame_getpc(efp);

    if (upc >= (uintptr_t)sigurgent && upc < (uintptr_t)sigurgent_end) {
        return false;
    }

    // This return address will be popped off the stack by the sigurgent()
    // function rts instruction.
    struct sigurgent_frame* fp = (struct sigurgent_frame*)usp_grow(sizeof(struct sigurgent_frame));
    fp->ret_addr = (void*)excpt_frame_getpc(efp);
    excpt_frame_setpc(efp, sigurgent);
    
    return true;
}


uintptr_t sp_grow(uintptr_t sp, size_t nbytes)
{
    sp -= nbytes;
    return sp;
}

void sp_shrink(uintptr_t sp, size_t nbytes)
{
    sp += nbytes;
}


bool cpu_is_null_fsave(const char* _Nonnull sfp)
{
    if (g_sys_desc->fpu_model != FPU_MODEL_68060) {
        return (((struct m6888x_null_frame*)sfp)->version == 0) ? true : false;
    }
    else {
        return (((struct m68060_fsave_frame*)sfp)->format == 0) ? true : false;
    }
}