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
const char* _Nonnull cpu_get_name(cpu_subtype_t cpu_subtype)
{
#define k68k_family_count 6
    static const char* g_68k_cpu_name[k68k_family_count] = {
        "68000", "68010", "68020", "68030", "68040", "68060"
    };

    const int family = cpu_68k_family(cpu_subtype) - 1;
    if (family < k68k_family_count) {
        return g_68k_cpu_name[family];
    }
    else {
        return "??";
    }
}

// Returns the model name of the FPU
const char* _Nonnull fpu_get_name(cpu_subtype_t cpu_subtype)
{
    #define k68k_fpu_count 5
    static const char* g_68k_fpu_name[k68k_fpu_count] = {
        "", "68881", "68882", "68040", "68060"
    };

    const int fpu = cpu_68k_fpu(cpu_subtype) - 1;
    if (fpu < k68k_fpu_count) {
        return g_68k_fpu_name[fpu];
    }
    else {
        return "??";
    }
}

void cpu_inject_sigurgent(excpt_frame_t* _Nonnull efp)
{
    struct sigurgent_frame {
        void* ret_addr;
    };

    extern void sig_urgent(void);
    extern void sig_urgent_end(void);
    const uintptr_t upc = excpt_frame_getpc(efp);

    if (upc >= (uintptr_t)sig_urgent && upc < (uintptr_t)sig_urgent_end) {
        return;
    }

    // This return address will be popped off the stack by the sig_urgent()
    // function rts instruction.
    struct sigurgent_frame* fp = (struct sigurgent_frame*)usp_grow(sizeof(struct sigurgent_frame));
    fp->ret_addr = (void*)excpt_frame_getpc(efp);
    excpt_frame_setpc(efp, sig_urgent);
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
    if (cpu_68k_fpu(g_sys_desc->cpu_subtype) != CPU_FPU_68060) {
        return (((struct m6888x_null_frame*)sfp)->version == 0) ? true : false;
    }
    else {
        return (((struct m68060_fsave_frame*)sfp)->format == 0) ? true : false;
    }
}