//
//  machine/arch/m68k/cpu.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/9/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <kern/assert.h>
#include <kern/string.h>
#include <kern/types.h>
#include <kpi/exception.h>
#include <machine/cpu.h>
#include <process/Process.h>


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

excpt_func_t _Nonnull cpu_exception(excpt_frame_t* _Nonnull efp, void* _Nullable sfp)
{
    vcpu_t vp = vcpu_current();
    fsave_frame_t* fpufp = sfp;
    excpt_info_t ei;
    excpt_ctx_t ec;

    if (!excpt_frame_isuser(efp)) {
        _fatalException(efp);
        /* NOT REACHED */
    }

    ei.cpu_code = excpt_frame_getvecnum(efp);
    ei.addr = NULL;

    switch (ei.cpu_code) {
        case EXCPT_NUM_ZERO_DIV:
            ei.code = EXCPT_DIV_ZERO;
            ei.addr = (void*)efp->u.f2.addr;
            break;

        case EXCPT_NUM_ILL_INSTR:
        case EXCPT_NUM_LINE_A:
        case EXCPT_NUM_LINE_F:
        case EXCPT_NUM_COPROC:
        case EXCPT_NUM_FORMAT:
        case EXCPT_NUM_EMU:
        case EXCPT_NUM_TRACE:
        case EXCPT_NUM_PRIV_VIO:
            ei.code = EXCPT_ILLEGAL;
            ei.addr = (void*)efp->pc;
            break;

        case EXCPT_NUM_CHK:
        case EXCPT_NUM_TRAPX:
            ei.code = EXCPT_TRAP;
            ei.addr = (void*)efp->u.f2.addr;
            break;

        case EXCPT_NUM_TRAP_0:
        case EXCPT_NUM_TRAP_1:
        case EXCPT_NUM_TRAP_2:
        case EXCPT_NUM_TRAP_3:
        case EXCPT_NUM_TRAP_4:
        case EXCPT_NUM_TRAP_5:
        case EXCPT_NUM_TRAP_6:
        case EXCPT_NUM_TRAP_7:
        case EXCPT_NUM_TRAP_8:
        case EXCPT_NUM_TRAP_9:
        case EXCPT_NUM_TRAP_10:
        case EXCPT_NUM_TRAP_11:
        case EXCPT_NUM_TRAP_12:
        case EXCPT_NUM_TRAP_13:
        case EXCPT_NUM_TRAP_14:
        case EXCPT_NUM_TRAP_15:
            ei.code = EXCPT_TRAP;
            ei.addr = (void*)efp->pc;
            break;

        case EXCPT_NUM_FPU_BR_UO:
        case EXCPT_NUM_FPU_INEXACT:
        case EXCPT_NUM_FPU_DIV_ZERO:
        case EXCPT_NUM_FPU_UNDERFLOW:
        case EXCPT_NUM_FPU_OP_ERR:
        case EXCPT_NUM_FPU_OVERFLOW:
        case EXCPT_NUM_FPU_SNAN:
        case EXCPT_NUM_FPU_UNIMPL_TY:
            ei.code = EXCPT_FPE;
            ei.addr = (void*)efp->pc;
            break;


        case EXCPT_NUM_BUS_ERR:
            ei.code = EXCPT_BUS;
            ei.addr = (void*)efp->pc;
            break;

        case EXCPT_NUM_ADR_ERR:
        case EXCPT_NUM_MMU_CONF_ERR:
        case EXCPT_NUM_MMU_ILL_OP:
        case EXCPT_NUM_MMU_ACCESS_VIO:
        case EXCPT_NUM_UNIMPL_EA:
        case EXCPT_NUM_UNIMPL_INT:
            ei.code = EXCPT_SEGV;
            ei.addr = (void*)efp->pc;  //XXX find real fault address
            break;

        default:
            _fatalException(efp);
            /* NOT REACHED */
    }

    if ((vp->flags & VP_FLAG_HANDLING_EXCPT) != 0) {
        // double fault -> exit
        Process_Exit(vp->proc, JREASON_EXCEPTION, ei.code);
        /* NOT REACHED */
    }

    return Process_Exception(vp->proc, vp, &ei, &ec);
}

void cpu_exception_return(void)
{
    vcpu_t vp = vcpu_current();

    Process_ExceptionReturn(vp->proc, vp);
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
void cpu_make_callout(mcontext_t* _Nonnull cp, void* _Nonnull ksp, void* _Nonnull usp, bool isUser, VoidFunc_1 _Nonnull func, void* _Nullable arg, VoidFunc_0 _Nonnull ret_func)
{
    // Initialize the CPU context:
    // Integer state: zeroed out
    // Floating-point state: establishes IEEE 754 standard defaults (non-signaling exceptions, round to nearest, extended precision)
    memset(cp, 0, sizeof(mcontext_t));
    cp->a[7] = (uintptr_t) ksp;
    cp->usp = (uintptr_t) usp;


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
    if (isUser) {
        uintptr_t usp = cp->usp;
        usp = sp_push_ptr(usp, arg);
        usp = sp_push_rts(usp, (void*)ret_func);
        cp->usp = usp;
    }
    else {
        uintptr_t ksp = cp->a[7];
        ksp = sp_push_ptr(ksp, arg);
        ksp = sp_push_rts(ksp, (void*)ret_func);
        cp->a[7] = ksp;
    }


    // Push a format #0 CPU exception frame on the kernel stack for the first
    // context switch.
    cp->a[7] -= sizeof(excpt_0_frame_t);
    excpt_0_frame_t* efp = (excpt_0_frame_t*)cp->a[7];
    efp->fv = 0;
    efp->pc = (uintptr_t)func;
    efp->sr = (isUser) ? 0 : CPU_SR_S;
}


uintptr_t sp_push_ptr(uintptr_t sp, void* _Nonnull ptr)
{
    uint32_t* p_sp = (uint32_t*)sp;

    p_sp--;
    *p_sp = (uint32_t)ptr;
    return (uintptr_t)p_sp;
}

uintptr_t sp_push_bytes(uintptr_t sp, const void* _Nonnull p, size_t nbytes)
{
    char* p_sp = (char*)sp;
    const char* p_src = (const char*)p;

    p_sp -= nbytes;
    if (((uintptr_t)p_sp & 1) != 0) {
        p_sp--;
    }
    uintptr_t nsp = (uintptr_t)p_sp;

    while (nbytes > 0) {
        *p_sp++ = *p_src++;
        nbytes--;
    }

    return nsp;
}
