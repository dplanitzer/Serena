//
//  machine/hw/m68k/cpu.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/9/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <hal/cpu.h>
#include <hal/sys_desc.h>


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