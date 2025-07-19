//
//  machine/arch/m68k/cpu.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/9/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "cpu.h"
#include <kern/string.h>
#include <kern/types.h>


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

// Sets up the provided CPU context and kernel/user stack with a function invocation
// frame that is suitable as the first frame that a VP will execute.
//
// \param cp CPU context
// \param ksp initial kernel stack pointer
// \param usp initial user stack pointer
// \param isUser true if 'func' is a user space function; false if it is a kernel space function
// \param func the function to invoke
// \param arg the pointer-sized argument to pass to 'func'
// \param ret_func the function that should be invoked when 'func' returns
void cpu_make_callout(CpuContext* _Nonnull cp, void* _Nonnull ksp, void* _Nonnull usp, bool isUser, VoidFunc_1 _Nonnull func, void* _Nullable arg, VoidFunc_0 _Nonnull ret_func)
{
    // Initialize the CPU context:
    // Integer state: zeroed out
    // Floating-point state: establishes IEEE 754 standard defaults (non-signaling exceptions, round to nearest, extended precision)
    memset(cp, 0, sizeof(CpuContext));
    cp->a[7] = (uintptr_t) ksp;
    cp->usp = (uintptr_t) usp;
    cp->pc = (uintptr_t) func;
    cp->sr = (isUser) ? 0 : 0x2000;


    // User stack:
    //
    // We push the argument and a rts return address that will invoke 'ret_func'
    // when the top-level user space function attempts to return.
    //
    //
    // Kernel stack:
    //
    // The initial kernel stack frame looks like this:
    // SP + 12: 'arg'
    // SP +  8: RTS address ('ret_func' entry point)
    // SP +  0: dummy format $0 exception stack frame (8 byte size)
    //
    // See __csw_rte_switch for an explanation
    // of why we need the dummy exception stack frame.
    if (isUser) {
        uint8_t* sp_u = (uint8_t*) cp->usp;
        sp_u -= 4; *((uint8_t**)sp_u) = arg;
        sp_u -= 4; *((uint8_t**)sp_u) = (uint8_t*)ret_func;
        cp->usp = (uintptr_t)sp_u;

        uint8_t* sp_k = (uint8_t*) cp->a[7];
        sp_k -= 4; *((uint32_t*)sp_k) = 0;
        sp_k -= 4; *((uint32_t*)sp_k) = 0;
        cp->a[7] = (uintptr_t)sp_k;
    }
    else {
        uint8_t* sp = (uint8_t*) cp->a[7];
        sp -= 4; *((uint8_t**)sp) = arg;
        sp -= 4; *((uint8_t**)sp) = (uint8_t*)ret_func;
        sp -= 4; *((uint32_t*)sp) = 0;
        sp -= 4; *((uint32_t*)sp) = 0;
        cp->a[7] = (uintptr_t)sp;
    }
}
