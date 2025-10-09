//
//  m68k/vcpu.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/21/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include <kern/limits.h>
#include <kern/string.h>
#include <sched/vcpu.h>


// Sets the closure which the virtual processor should run when it is resumed.
// This function may only be called while the VP is suspended.
//
// \param self the virtual processor
// \param closure the closure description
// \param bEnableInterrupts true if IRQs should be enabled; false if disabled
errno_t vcpu_setcontext(vcpu_t _Nonnull self, const VirtualProcessorClosure* _Nonnull closure, bool bEnableInterrupts)
{
    decl_try_err();

    VP_ASSERT_ALIVE(self);
    assert(self->suspension_count > 0);
    assert(closure->kernelStackSize >= VP_MIN_KERNEL_STACK_SIZE);

    if (closure->kernelStackBase == NULL) {
        err = stk_setmaxsize(&self->kernel_stack, closure->kernelStackSize);
    } else {
        stk_setmaxsize(&self->kernel_stack, 0);
        self->kernel_stack.base = closure->kernelStackBase;
        self->kernel_stack.size = closure->kernelStackSize;
    }
    if (err == EOK) {
        err = stk_setmaxsize(&self->user_stack, closure->userStackSize);
    }
    if (err != EOK) {
        return err;
    }


    // Initialize the CPU context:
    // Integer state: zeroed out
    // Floating-point state: establishes IEEE 754 standard defaults (non-signaling exceptions, round to nearest, extended precision)
    VoidFunc_0 ret_func = (closure->ret_func) ? closure->ret_func : (VoidFunc_0)vcpu_relinquish;
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
    // See __csw_rte_switch for an explanation of why we need to push a format #0
    // exception stack frame here.
    if (closure->isUser) {
        usp = sp_push_ptr(usp, closure->context);
        usp = sp_push_rts(usp, (void*)ret_func);
    }
    else {
        ksp = sp_push_ptr(ksp, closure->context);
        ksp = sp_push_rts(ksp, (void*)ret_func);
    }


    // Initial save area (high to low addresses):
    // SP-0:    exception frame type    \
    // SP-2:    PC                      |   Exception stack frame type #0
    // SP-6:    SR                      /
    // SP-8:    a[7]                        a0 to a6
    // SP-36:   d[8]                        d0 to d7
    // SP-68:   usp                         user stack pointer
    // SP-72:   fpu_save                    fsave frame, 4 to 400 bytes (all following offsets assume NULL fsave frame of 4 bytes)
    // SP-76:   fp[8]                       fp0 to fp7
    // SP-172:  fpcr
    // SP-176:  fpsr
    // SP-180:  fpiar
    const size_t ifsiz = sizeof(excpt_0_frame_t) + 4*7 + 4*8 + 4;
    const size_t ffsiz = 4 + sizeof(float96_t)*8 + 4 + 4 + 4;
    const size_t fsiz = ifsiz + (((self->flags & VP_FLAG_FPU) == VP_FLAG_FPU) ? ffsiz : 0);
    uintptr_t ssp = ksp - fsiz;
    memset((void*)ssp, 0, fsiz);

    // Push a format #0 CPU exception frame on the kernel stack for the first
    // context switch.
    excpt_0_frame_t* ep = (excpt_0_frame_t*)(ksp - sizeof(excpt_0_frame_t));
    ep->fv = 0;
    ep->pc = (uintptr_t)closure->func;
    ep->sr = (closure->isUser) ? 0 : CPU_SR_S;
    if (!bEnableInterrupts) {
        ep->sr |= 0x0700;    // IRQs should be disabled
    }

    uint32_t* uspp = (int32_t*)(ksp - ifsiz);
    uspp[0] = usp;
    
    self->ssp = (void*)ssp;

    return EOK;
}
