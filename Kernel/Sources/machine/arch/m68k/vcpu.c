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
errno_t vcpu_setcontext(vcpu_t _Nonnull self, const vcpu_context_t* _Nonnull closure, bool bEnableInterrupts)
{
    decl_try_err();
    const size_t ifsiz = sizeof(excpt_0_frame_t) + 4*7 + 4*8 + 4;
    const size_t ffsiz = 4 + sizeof(float96_t)*8 + 4 + 4 + 4;
    const bool hasFPU = (self->flags & VP_FLAG_HAS_FPU) == VP_FLAG_HAS_FPU;

    VP_ASSERT_ALIVE(self);
    assert(self->suspension_count > 0);

    // Minimum kernel stack size is 2 * sizeof(cpu_save_area_max_size) + 128
    // 2 times -> csw save state + cpu exception save state
    const size_t minKernelStackSize = self->kernel_stack.size >= 2*(ifsiz + ffsiz + FPU_MAX_STATE_SIZE) + 128;
    const size_t minUserStackSize = (closure->userStackSize != 0) ? 2048 : 0;

    if (closure->kernelStackBase == NULL) {
        err = stk_setmaxsize(&self->kernel_stack, __max(closure->kernelStackSize, minKernelStackSize));
    } else {
        // kernel stack allocated by caller
        assert(closure->kernelStackSize >= minKernelStackSize);

        stk_setmaxsize(&self->kernel_stack, 0);
        self->kernel_stack.base = closure->kernelStackBase;
        self->kernel_stack.size = closure->kernelStackSize;
    }
    if (err == EOK) {
        err = stk_setmaxsize(&self->user_stack, __max(closure->userStackSize, minUserStackSize));
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
    // See __csw_switch for an explanation of why we need to push a format #0
    // exception stack frame here.
    if (closure->isUser) {
        usp = sp_push_ptr(usp, closure->context);
        usp = sp_push_rts(usp, (void*)ret_func);
    }
    else {
        ksp = sp_push_ptr(ksp, closure->context);
        ksp = sp_push_rts(ksp, (void*)ret_func);
    }


    // General save area layout (high to low addresses):
    // ksp-0:    exception frame type    \
    // ksp-2:    pc                      |  Exception stack frame type #0
    // ksp-6:    sr                      /
    // ksp-8:    a[7]                       a0 to a6
    // ksp-36:   d[8]                       d0 to d7
    // ksp-68:   usp                        user stack pointer
    // ksp-72:   fpu_save                   fsave frame, 4 to 216 bytes (all following offsets assume NULL fsave frame of 4 bytes)
    // ksp-76:   fp[8]                      fp0 to fp7
    // ksp-172:  fpcr
    // ksp-176:  fpsr
    // ksp-180:  fpiar
    // -------------------------------------------------------------------------
    //
    // Initial save area layout (high to low addresses):
    // ksp-0:    exception frame type    \
    // ksp-2:    pc                      |  Exception stack frame type #0
    // ksp-6:    sr                      /
    // ksp-8:    a[7]                       a0 to a6
    // ksp-36:   d[8]                       d0 to d7
    // ksp-68:   usp                        user stack pointer
    // ksp-72:   fpu_save                   fsave NULL frame, 4 bytes (this causes the FPU to reset its user state)
    const size_t fsiz = ifsiz + ((hasFPU) ? sizeof(struct m6888x_null_frame) : 0);
    uintptr_t ssp = ksp - fsiz;
    memset((void*)ssp, 0, fsiz);

    // Push a format #0 CPU exception frame on the kernel stack for the first
    // context switch.
    excpt_0_frame_t* ep = (excpt_0_frame_t*)(ksp - sizeof(excpt_0_frame_t));
    ep->fv = 0;
    ep->pc = (uintptr_t)closure->func;
    ep->sr = (closure->isUser) ? 0 : CPU_SR_S;
    if (!bEnableInterrupts) {
        ep->sr |= CPU_SR_IE_MASK;   // IRQs should be disabled
    }

    uint32_t* uspp = (int32_t*)(ksp - ifsiz);
    uspp[0] = usp;
    
    self->ssp = (void*)ssp;

    return EOK;
}
