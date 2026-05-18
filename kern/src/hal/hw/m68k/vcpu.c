//
//  m68k/vcpu.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/21/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include <assert.h>
#include <limits.h>
#include <string.h>
#include <ext/math.h>
#include <sched/vcpu.h>

extern void fpu_idle_fsave(char* _Nonnull fsave);


static char g_fpu_idle_fsave[FPU_MAX_FSAVE_SIZE];

void vcpu_platform_init(void)
{
    if (cpu_68k_fpu(g_sys_desc->cpu_subtype) > CPU_FPU_NONE) {
        fpu_idle_fsave(g_fpu_idle_fsave);
    }
}

// Returns the required minimum kernel stack size
size_t min_vcpu_kernel_stack_size(void)
{
    // Minimum kernel stack size is 4 * sizeof(cpu_save_area_max_size) + 256
    // 4x -> syscall + cpu exception + cpu exception (double fault) + csw
    return 4*(sizeof(excpt_frame_t) + sizeof(cpu_full_state_t)) + 256;
}

void _vcpu_reset_stacks(vcpu_t _Nonnull self, VoidFunc_1 _Nonnull func, void* _Nullable _Weak arg, VoidFunc_0 _Nonnull ret_func, bool isUser, bool bEnableInterrupts)
{
    struct func_frame {
        void* ret_addr;
        void* arg;
    };

    assert(func != NULL);
    assert(ret_func != NULL);


    // Initialize the CPU context:
    // Integer state: zeroed out
    // Floating-point state: establishes IEEE 754 standard defaults (non-signaling exceptions, round to nearest, extended precision)
    uintptr_t ksp = (uintptr_t) stk_getinitialsp(&self->kernel_stack);
    uintptr_t usp = (uintptr_t) stk_getinitialsp(&self->user_stack);


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
    // SP +  0: format #0 CPU exception stack frame (8 byte size)
    //
    // See __sched_switch_context for an explanation of why we need to push a
    // format #0 exception stack frame here.
    struct func_frame* fp;
    if (isUser) {
        usp = sp_grow(usp, sizeof(struct func_frame));
        fp = (struct func_frame*)usp;
    }
    else {
        ksp = sp_grow(ksp, sizeof(struct func_frame));
        fp = (struct func_frame*)ksp;
    }
    fp->arg = arg;
    fp->ret_addr = (void*)ret_func;


    // Create the initial context switch state. This state is stored on the
    // kernel stack and will be consumed by the first context switch to this
    // new vcpu.
    // 
    // General save area layout (high to low addresses):
    // ksp-0:    exception frame type    |
    // ksp-2:    pc                      |  Exception stack frame type #0
    // ksp-6:    sr                      |
    // ksp-8:    a[7]                       a6 to a0
    // ksp-36:   d[8]                       d7 to d0
    // ksp-68:   usp                        user stack pointer
    // ksp-72:   fpu_save[216]              fsave frame storage area of 216 bytes
    // ksp-288:  fp[8]                      fp7 to fp0
    // ksp-384:  fpcr
    // ksp-388:  fpsr
    // ksp-392:  fpiar
    // ################     <--- kernel stack pointer
    // -------------------------------------------------------------------------
    //
    // Initial save area layout (high to low addresses):
    // ksp-0:    exception frame type    |
    // ksp-2:    pc                      |  Exception stack frame type #0
    // ksp-6:    sr                      |
    // ksp-8:    a[7]                       a6 to a0
    // ksp-36:   d[8]                       d7 to d0
    // ksp-68:   usp                        user stack pointer
    // ksp-72:   fpu_save[216]              fsave NULL frame, 4 bytes (this causes the FPU to reset its user state)
    // ksp-288:  fp[8]                      0 (dummy value, not used since we frestore a NULL frame)
    // ksp-384:  fpcr                       0 (dummy value, not used since we frestore a NULL frame)
    // ksp-388:  fpsr                       0 (dummy value, not used since we frestore a NULL frame)
    // ksp-392:  fpiar                      0 (dummy value, not used since we frestore a NULL frame)
    // ################     <--- kernel stack pointer
    cpu_full_state_t* csw_sa = (cpu_full_state_t*)(ksp - sizeof(cpu_full_state_t));
    memset(csw_sa, 0, sizeof(cpu_full_state_t));

    // We do not setup the kernel stack pointer (a[7]) here because the csw
    // restore loads a[7] from vp->csw_sa
    csw_sa->b.usp = usp;
    csw_sa->b.ef.fv = 0;
    csw_sa->b.ef.pc = (uintptr_t)func;
    csw_sa->b.ef.sr = (isUser) ? 0 : CPU_SR_S;
    if (!bEnableInterrupts) {
        csw_sa->b.ef.sr |= CPU_SR_IE_MASK;   // IRQs should be disabled
    }
    
    self->csw_sa = csw_sa;
}


void _cpu_set_basic_state(cpu_basic_state_t* _Nonnull dp, const vcpu_state_m68k_t* _Nonnull sp)
{
    // See _cpu_set_basic_state().
    for (int i = 0; i < 7; i++) {
        dp->a[i] = sp->a[i];
        dp->d[i] = sp->d[i];
    }
    dp->usp = sp->a[7];
    dp->d[7] = sp->d[7];

    dp->ef.pc = sp->pc;
    dp->ef.sr &= 0xff00;
    dp->ef.sr |= sp->sr & 0xff;     // update CCR only
}

void _cpu_set_float_state(cpu_float_state_t* _Nonnull dp, const vcpu_state_m68k_float_t* _Nonnull sp)
{
    dp->fpcr = sp->fpcr;
    dp->fpiar = sp->fpiar;
    dp->fpsr = sp->fpsr;

    for (int i = 0; i < 8; i++) {
        dp->fp[i] = sp->fp[i];
    }


    // Replace the old fsave with an idle fsave. This ensures that eg a
    // deferred exception that might have been triggered by the previous FPU
    // state will be abandoned.
    memcpy(dp->fsave, g_fpu_idle_fsave, FPU_MAX_FSAVE_SIZE);
}


void _cpu_get_basic_state(vcpu_state_m68k_t* _Nonnull dp, const cpu_basic_state_t* _Nonnull sp)
{
    for (int i = 0; i < 7; i++) {
        dp->a[i] = sp->a[i];
        dp->d[i] = sp->d[i];
    }
    dp->a[7] = sp->usp;
    dp->d[7] = sp->d[7];

    dp->pc = sp->ef.pc;
    dp->sr = sp->ef.sr & 0x00ff;      // read CCR only
}

void _cpu_get_float_state(vcpu_state_m68k_float_t* _Nonnull dp, const cpu_float_state_t* _Nonnull sp)
{
    if (!cpu_is_null_fsave(&sp->fsave[0])) {
        dp->fpcr = sp->fpcr;
        dp->fpiar = sp->fpiar;
        dp->fpsr = sp->fpsr;

        for (int i = 0; i < 8; i++) {
            dp->fp[i] = sp->fp[i];
        }
    }
    else {
        dp->fpcr = 0;
        dp->fpiar = 0;
        dp->fpsr = 0;

        for (int i = 0; i < 8; i++) {
            dp->fp[i] = (float96_t){0};
        }
    }
}
