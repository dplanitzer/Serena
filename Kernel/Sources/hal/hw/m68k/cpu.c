//
//  machine/hw/m68k/cpu.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/9/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <hal/cpu.h>
#include <kern/assert.h>
#include <kern/string.h>
#include <kern/types.h>
#include <kpi/exception.h>
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


static bool map_exception(int cpu_code, excpt_frame_t* _Nonnull efp, excpt_info_t* _Nonnull ei)
{
    switch (cpu_code) {
        case EXCPT_NUM_ZERO_DIV:
            ei->code = EXCPT_DIV_ZERO;
            ei->addr = (void*)efp->u.f2.addr;
            break;

        case EXCPT_NUM_ILL_INSTR:
        case EXCPT_NUM_LINE_A:
        case EXCPT_NUM_LINE_F:
        case EXCPT_NUM_FORMAT:
        case EXCPT_NUM_EMU:
        case EXCPT_NUM_TRACE:
        case EXCPT_NUM_PRIV_VIO:
            ei->code = EXCPT_ILLEGAL;
            ei->addr = (void*)efp->pc;
            break;

        case EXCPT_NUM_CHK:
        case EXCPT_NUM_TRAPX:
            ei->code = EXCPT_TRAP;
            ei->addr = (void*)efp->u.f2.addr;
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
            ei->code = EXCPT_TRAP;
            ei->addr = (void*)efp->pc;
            break;

        case EXCPT_NUM_FPU_BRANCH_UO:
        case EXCPT_NUM_FPU_INEXACT:
        case EXCPT_NUM_FPU_DIV_ZERO:
        case EXCPT_NUM_FPU_UNDERFLOW:
        case EXCPT_NUM_FPU_OP_ERR:
        case EXCPT_NUM_FPU_OVERFLOW:
        case EXCPT_NUM_FPU_SNAN:
        case EXCPT_NUM_FPU_UNIMPL_TY:
            ei->code = EXCPT_FPE;
            ei->addr = (void*)efp->pc;
            break;


        case EXCPT_NUM_BUS_ERR:
            ei->code = EXCPT_BUS;
            ei->addr = (void*)efp->pc;
            break;

        case EXCPT_NUM_ADR_ERR:
        case EXCPT_NUM_MMU_CONF_ERR:
        case EXCPT_NUM_MMU_ILL_OP:
        case EXCPT_NUM_MMU_ACCESS_VIO:
        case EXCPT_NUM_UNIMPL_EA:
        case EXCPT_NUM_UNIMPL_INT:
            ei->code = EXCPT_SEGV;
            ei->addr = (void*)efp->pc;  //XXX find real fault address
            break;

        case EXCPT_NUM_COPROC:
            // coprocessor protocol violations are fatal
        default:
            return false;
    }

    ei->cpu_code = cpu_code;

    return true;
}

// User exception frame layout before entering the user exception handler
struct u_excpt_frame {
    void*           ret_addr;
    void*           arg;
    excpt_info_t*   ei_ptr;
    mcontext_t*     mc_ptr;

    mcontext_t      mc;
    excpt_info_t    ei;
};

// User exception frame layout after exiting the user exception handler.
struct u_excpt_frame_ret {
    void*           arg;
    excpt_info_t*   ei_ptr;
    mcontext_t*     mc_ptr;

    mcontext_t      mc;
    excpt_info_t    ei;
};


extern void _vcpu_write_excpt_mcontext(vcpu_t _Nonnull self, const mcontext_t* _Nonnull ctx);
extern void _vcpu_read_excpt_mcontext(vcpu_t _Nonnull self, mcontext_t* _Nonnull ctx);

void cpu_exception(struct vcpu* _Nonnull vp, excpt_0_frame_t* _Nonnull utp)
{
    void* ksp = ((char*)utp) + sizeof(excpt_0_frame_t);
    excpt_frame_t* efp = (excpt_frame_t*)&vp->excpt_sa->ef;
    const int cpu_code = excpt_frame_getvecnum(efp);
    excpt_info_t ei;
    excpt_handler_t eh;

    // Any exception triggered in kernel mode
    if (!excpt_frame_isuser(efp)) {
        _fatalException(ksp);
        /* NOT REACHED */
    }


    if (!map_exception(cpu_code, efp, &ei)) {
        _fatalException(ksp);
        /* NOT REACHED */
    }


    // MC68881/MC68882 User's Manual, page 5-10
    // 68060UM, page 6-37
    if (ei.code == EXCPT_FPE) {
        switch (g_sys_desc->fpu_model) {
            case FPU_MODEL_68882: {
                struct m68882_idle_frame* idle_p = (struct m68882_idle_frame*)vp->excpt_sa->fsave;

                if (idle_p->format == FSAVE_FORMAT_882_IDLE) {
                    idle_p->biu_flags |= BIU_FP_EXCPT_PENDING;
                }
                break;
            }

            case FPU_MODEL_68060: {
                struct m68060_fsave_frame* fsave_p = (struct m68060_fsave_frame*)vp->excpt_sa->fsave;

                if (fsave_p->format == FSAVE_FORMAT_68060_EXCP) {
                    fsave_p->format = FSAVE_FORMAT_68060_IDLE;
                }
                break;
            }
        }
    }


    if (vp->excpt_id > 0 || !Process_ResolveExceptionHandler(vp->proc, vp, &eh)) {
        // double fault or no exception handler -> exit
        Process_Exit(vp->proc, JREASON_EXCEPTION, ei.code);
        /* NOT REACHED */
    }


    // Record the active exception type
    vp->excpt_id = ei.code;


    // Push the exception info on the user stack
    struct u_excpt_frame* uep = (struct u_excpt_frame*)usp_grow(sizeof(struct u_excpt_frame));
    _vcpu_read_excpt_mcontext(vp, &uep->mc);
    uep->ei = ei;
    uep->ei_ptr = &uep->ei;
    uep->mc_ptr = &uep->mc;
    uep->arg = eh.arg;
    uep->ret_addr = (void*)excpt_return;


    // Update the u-trampoline with the exception function entry point
    utp->pc = (uintptr_t)eh.func;
}

void cpu_exception_return(struct vcpu* _Nonnull vp)
{
    struct u_excpt_frame_ret* usp = (struct u_excpt_frame_ret*)usp_get();

    // Write back the (possibly) updated machine context
    _vcpu_write_excpt_mcontext(vp, usp->mc_ptr);

    // Pop the exception info off the user stack. Note that the return address
    // was already taken off by the CPU before we came here
    usp_shrink(sizeof(struct u_excpt_frame_ret));

    // This vcpu is no longer processing an exception
    vp->excpt_id = 0;
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