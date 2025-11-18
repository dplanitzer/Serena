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


// Returns the required minimum kernel stack size
size_t min_vcpu_kernel_stack_size(void)
{
    const size_t ie_sa_siz = sizeof(excpt_frame_t) + sizeof(cpu_savearea_t);
    const size_t f_sa_siz = sizeof(fpu_savearea_t) + FPU_MAX_FSAVE_SIZE;

    // Minimum kernel stack size is 3 * sizeof(cpu_save_area_max_size) + 256
    // 3x -> csw save state + syscall save state + cpu exception save state
    return 3*(ie_sa_siz + f_sa_siz) + 256;
}

// Sets the closure which the virtual processor should run when it is next resumed.
//
// \param self the virtual processor
// \param act the activation record
// \param bEnableInterrupts true if IRQs should be enabled; false if disabled
errno_t vcpu_setcontext(vcpu_t _Nonnull self, const vcpu_acquisition_t* _Nonnull ac, bool bEnableInterrupts)
{
    decl_try_err();
    const size_t minKernelStackSize = min_vcpu_kernel_stack_size();
    const size_t minUserStackSize = (ac->userStackSize != 0) ? 2048 : 0;

    if (ac->kernelStackBase == NULL) {
        err = stk_setmaxsize(&self->kernel_stack, __max(ac->kernelStackSize, minKernelStackSize));
    } else {
        // kernel stack allocated by caller
        assert(ac->kernelStackSize >= minKernelStackSize);

        stk_setmaxsize(&self->kernel_stack, 0);
        self->kernel_stack.base = ac->kernelStackBase;
        self->kernel_stack.size = ac->kernelStackSize;
    }
    if (err == EOK) {
        err = stk_setmaxsize(&self->user_stack, __max(ac->userStackSize, minUserStackSize));
    }
    if (err != EOK) {
        return err;
    }


    // Initialize the CPU context:
    // Integer state: zeroed out
    // Floating-point state: establishes IEEE 754 standard defaults (non-signaling exceptions, round to nearest, extended precision)
    VoidFunc_0 ret_func = (ac->ret_func) ? ac->ret_func : (VoidFunc_0)vcpu_relinquish;
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
    if (ac->isUser) {
        usp = sp_push_ptr(usp, ac->arg);
        usp = sp_push_rts(usp, (void*)ret_func);
    }
    else {
        ksp = sp_push_ptr(ksp, ac->arg);
        ksp = sp_push_rts(ksp, (void*)ret_func);
    }


    // General save area layout (high to low addresses):
    // ksp-0:    exception frame type    \
    // ksp-2:    pc                      |  Exception stack frame type #0
    // ksp-6:    sr                      /
    // ksp-8:    a[7]                       a6 to a0
    // ksp-36:   d[8]                       d7 to d0
    // ksp-68:   usp                        user stack pointer
    // ksp-72:   fpu_save                   fsave frame, 4 to 216 bytes (all following offsets assume NULL fsave frame of 4 bytes)
    // ksp-76:   fp[8]                      fp7 to fp0
    // ksp-172:  fpcr
    // ksp-176:  fpsr
    // ksp-180:  fpiar
    // ################     <--- kernel stack pointer
    // -------------------------------------------------------------------------
    //
    // Initial save area layout (high to low addresses):
    // ksp-0:    exception frame type    \
    // ksp-2:    pc                      |  Exception stack frame type #0
    // ksp-6:    sr                      /
    // ksp-8:    a[7]                       a6 to a0
    // ksp-36:   d[8]                       d7 to d0
    // ksp-68:   usp                        user stack pointer
    // ksp-72:   fpu_save                   fsave NULL frame, 4 bytes (this causes the FPU to reset its user state)
    // ksp-76:   fp[8]                      0 (dummy value, not used since we frestore a NULL frame)
    // ksp-172:  fpcr                       0 (dummy value, not used since we frestore a NULL frame)
    // ksp-176:  fpsr                       0 (dummy value, not used since we frestore a NULL frame)
    // ksp-180:  fpiar                      0 (dummy value, not used since we frestore a NULL frame)
    // ################     <--- kernel stack pointer
    const size_t ie_sa_siz = sizeof(excpt_0_frame_t) + sizeof(cpu_savearea_t);
    const bool hasFPU = (self->flags & VP_FLAG_HAS_FPU) == VP_FLAG_HAS_FPU;
    const size_t frm_siz = ie_sa_siz + ((hasFPU) ? sizeof(fpu_savearea_t) : 0);
    uintptr_t csw_sa = ksp - frm_siz;
    memset((void*)csw_sa, 0, frm_siz);

    // Push a format #0 CPU exception frame on the kernel stack for the first
    // context switch.
    excpt_0_frame_t* ep = (excpt_0_frame_t*)(ksp - sizeof(excpt_0_frame_t));
    ep->fv = 0;
    ep->pc = (uintptr_t)ac->func;
    ep->sr = (ac->isUser) ? 0 : CPU_SR_S;
    if (!bEnableInterrupts) {
        ep->sr |= CPU_SR_IE_MASK;   // IRQs should be disabled
    }

    uint32_t* uspp = (int32_t*)(ksp - ie_sa_siz);
    uspp[0] = usp;
    
    self->csw_sa = (void*)csw_sa;

    return EOK;
}

void _vcpu_write_mcontext(vcpu_t _Nonnull self, const mcontext_t* _Nonnull ctx)
{
    cpu_savearea_t* cpu_sa = (self->syscall_sa) ? self->syscall_sa : self->csw_sa;
    excpt_0_frame_t* excpt_sa = (excpt_0_frame_t*)(((char*)cpu_sa) + sizeof(cpu_savearea_t));
    fpu_savearea_t* fpu_sa = self->csw_sa;
    const bool hasFPU = (self->flags & VP_FLAG_HAS_FPU) == VP_FLAG_HAS_FPU;

    // See _vcpu_read_mcontext().
    for (int i = 0; i < 7; i++) {
        cpu_sa->a[i] = ctx->a[i];
        cpu_sa->d[i] = ctx->d[i];
    }
    cpu_sa->usp = ctx->a[7];
    cpu_sa->d[7] = ctx->d[7];

    excpt_sa->pc = ctx->pc;
    excpt_sa->sr &= 0xff00;
    excpt_sa->sr |= ctx->sr & 0xff;


    // Set the FPU state
    if (hasFPU) {
        fpu_sa->fpcr = ctx->fpcr;
        fpu_sa->fpiar = ctx->fpiar;
        fpu_sa->fpsr = ctx->fpsr;

        for (int i = 0; i < 8; i++) {
            fpu_sa->fp[i] = ctx->fp[i];
        }

        // XXX
        // Need to do something here if the fsave frame is a NULL frame. Eg promote
        // it to IDLE (although the exact format isn't documented); or make it so
        // that we'll never get a NULL frame in the first place by executing at least
        // one fp instruction when the vcpu is acquired
        // XXX
    }
}

void _vcpu_read_mcontext(vcpu_t _Nonnull self, mcontext_t* _Nonnull ctx)
{
    const cpu_savearea_t* cpu_sa = (self->syscall_sa) ? self->syscall_sa : self->csw_sa;
    const excpt_0_frame_t* excpt_sa = (excpt_0_frame_t*)(((char*)cpu_sa) + sizeof(cpu_savearea_t));
    const fpu_savearea_t* fpu_sa = self->csw_sa;
    const bool hasFPU = (self->flags & VP_FLAG_HAS_FPU) == VP_FLAG_HAS_FPU;

    // Get the integer state from the syscall save area if it exists and the
    // context switch save area otherwise. The CSW save area holds the kernel
    // integer state if we're inside a system call. The system call save area
    // holds the user integer state. The FPU state is always stored in the
    // CSW save area. System calls don't save the FPU state since the kernel
    // doesn't use it.
    for (int i = 0; i < 7; i++) {
        ctx->a[i] = cpu_sa->a[i];
        ctx->d[i] = cpu_sa->d[i];
    }
    ctx->a[7] = cpu_sa->usp;
    ctx->d[7] = cpu_sa->d[7];

    ctx->pc = excpt_sa->pc;
    ctx->sr = excpt_sa->sr & 0xff;


    // Get the FPU state
    if (hasFPU && !fsave_frame_isnull(&fpu_sa->fsave_hdr)) {
        ctx->fpcr = fpu_sa->fpcr;
        ctx->fpiar = fpu_sa->fpiar;
        ctx->fpsr = fpu_sa->fpsr;

        for (int i = 0; i < 8; i++) {
            ctx->fp[i] = fpu_sa->fp[i];
        }
    }
    else {
        ctx->fpcr = 0;
        ctx->fpiar = 0;
        ctx->fpsr = 0;

        for (int i = 0; i < 8; i++) {
            ctx->fp[i] = (float96_t){0};
        }
    }
}
