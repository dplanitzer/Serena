//
//  machine/hw/m68k/cpu_excpt.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/9/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <hal/cpu.h>
#include <kern/kernlib.h>
#include <kpi/exception.h>
#include <process/Process.h>


// User exception frame layout before entering the user exception handler
struct u_excpt_frame {
    void*           ret_addr;
    void*           arg;
    excpt_info_t*   ei_ptr;
    mcontext_t*     mc_ptr;

    mcontext_t      mc;     // only filled in if EXCPT_MCTX set
    excpt_info_t    ei;
};

// User exception frame layout after exiting the user exception handler.
struct u_excpt_frame_ret {
    void*           arg;
    excpt_info_t*   ei_ptr;
    mcontext_t*     mc_ptr;

    mcontext_t      mc;     // only filled in if EXCPT_MCTX set
    excpt_info_t    ei;
};


extern void _vcpu_write_excpt_mcontext(vcpu_t _Nonnull self, const mcontext_t* _Nonnull ctx);
extern void _vcpu_read_excpt_mcontext(vcpu_t _Nonnull self, mcontext_t* _Nonnull ctx);

// Used by cpu_asm.s
int8_t g_excpt_frame_size[16] = {8, 8, 12, 12, 12, 0, 0, 60, 0, 20, 32, 92, 0, 0, 0, 0};


static int get_ecode(int cpu_model, int cpu_code, excpt_frame_t* _Nonnull efp)
{
    switch (cpu_code) {
        case EXCPT_NUM_BUS_ERR:     // MC68040, MC68060: Access Fault
            if ((cpu_code == CPU_MODEL_68040 && excpt_frame_getformat(efp) == 7 && (efp->u.f7.ssw & SSW7_MA) == SSW7_MA)
                || (cpu_code == CPU_MODEL_68060 && excpt_frame_getformat(efp) == 4) && fslw_is_misaligned_rmw(efp->u.f4_access_error.fslw)) {
                return EXCPT_DATA_MISALIGNED;
            }
            else {
                return EXCPT_PAGE_ERROR;
            }

        case EXCPT_NUM_ADR_ERR:
            return EXCPT_INSTRUCTION_MISALIGNED;

        case EXCPT_NUM_ILLEGAL:
        case EXCPT_NUM_LINE_A:
        case EXCPT_NUM_PMMU_ACCESS:     // MC68851 PMMU is turned off and user space tries ot execute a PVALID instruction
        case EXCPT_NUM_UNIMPL_EA:       // MC68060  (TBD) -> 68060SP
        case EXCPT_NUM_UNIMPL_INST:     // MC68060  (TBD) -> 68060SP
        case EXCPT_NUM_EMU_INT:         // MC68060  (TBD) -> 68060SP
            return EXCPT_ILLEGAL_INSTRUCTION;

        case EXCPT_NUM_LINE_F:
            if (cpu_model < 68060 || (cpu_model >= CPU_MODEL_68060 && excpt_frame_getformat(efp) == 4)) {
                // Either a < 68060 CPU with no FPU present (e.g. 68LC040 or 68030 with no 68881/68882 co-proc)
                // or a MC68060 class CPU with FPU disabled or a MC68LC060/MC68EC060 (no FPU)
                return EXCPT_ILLEGAL_INSTRUCTION;
            }
            else {
                // (TBD) if MC68040 then -> 68040FPSP; if MC68060 then -> 68060SP
                return EXCPT_ILLEGAL_INSTRUCTION;
            }

        case EXCPT_NUM_ZERO_DIV:
            return EXCPT_INT_DIVIDE_BY_ZERO;

        case EXCPT_NUM_PRIV_VIO:
            return EXCPT_PRIV_INSTRUCTION;

        case EXCPT_NUM_TRACE:
            return EXCPT_SINGLE_STEP;

        case EXCPT_NUM_CHK:
            return EXCPT_BOUNDS_EXCEEDED;

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
            return EXCPT_SOFT_INTERRUPT;

        case EXCPT_NUM_TRAPcc:      // MC68881, MC68882, MC68851
            if (excpt_frame_getformat(efp) == 2 && *((uint16_t*)efp->u.f2.addr) == 0xCE76 /*TRAPV*/) {
                return EXCPT_INT_OVERFLOW;
            }
            else {
                return EXCPT_SOFT_INTERRUPT;
            }

        case EXCPT_NUM_FPU_BRANCH_UO:
        case EXCPT_NUM_FPU_SNAN:
            return EXCPT_FLT_NAN;

        case EXCPT_NUM_FPU_INEXACT:
            return EXCPT_FLT_INEXACT;

        case EXCPT_NUM_FPU_DIV_ZERO:
            return EXCPT_FLT_DIVIDE_BY_ZERO;

        case EXCPT_NUM_FPU_UNDERFLOW:
            return EXCPT_FLT_UNDERFLOW;

        case EXCPT_NUM_FPU_OP_ERR:
        case EXCPT_NUM_FPU_UNIMPL_TY:   // MC68040
            return EXCPT_FLT_OPERAND;

        case EXCPT_NUM_FPU_OVERFLOW:
            return EXCPT_FLT_OVERFLOW;

        case EXCPT_NUM_UNINIT_IRQ:
        case EXCPT_NUM_SPUR_IRQ:
        case EXCPT_NUM_IRQ_7:           // NMI
        case EXCPT_NUM_COPROC:          // MC68881, MC68882, MC68851
        case EXCPT_NUM_FORMAT:
        case EXCPT_NUM_MMU_CONFIG:      // MC68030 MMU, MC68851 PMMU
        case EXCPT_NUM_PMMU_ILLEGAL:    // MC68851 PMMU
        default:
            // any of these exceptions imply:
            // - buggy kernel code (e.g. bug in MMU config code)
            // - corrupted kernel memory
            // - hardware fault
            // - unknow exception type
            // we'll halt the system
            // fall through
            return -1;
    }
}

static uintptr_t get_faddr(int cpu_model, const excpt_frame_t* _Nonnull efp)
{
    // MC68020UM, p6-27 (152)ff
    switch (excpt_frame_getformat(efp)) {
        case 0x0:
        case 0x1:
            return efp->pc;

        case 0x2:
            return efp->u.f2.addr;

        case 0x3:
            return efp->u.f3.ea;

        case 0x4:
            if (cpu_model == CPU_MODEL_68040) {
                // MC68LC040 (no FPU)
                // MC68EC040 (no FPU, no MMU)
                // We return the PC of the faulted FP instruction to align us with
                // the standard illegal instruction exception type
                return efp->u.f4_line_f.pcFaultedInstr;
            }
            else if (cpu_model >= CPU_MODEL_68060) {
                return efp->u.f4_access_error.faddr;
            }
            else {
                return 0;
            }

        case 0x7:
            return efp->u.f7.fa;

        case 0x9:
            return efp->u.f9.ia;

        case 0xA:
        case 0xB:
            // format $A is a subset of format $B
            if (sswab_is_datafault(efp->u.fa.ssw)) {
                return efp->u.fa.dataCycleFaultAddress;
            }
            else {
                return efp->pc;
            }
            break;

        default:
            return 0;
    }
}

static void fp_fsave_fixup(struct vcpu* _Nonnull vp)
{
    if (g_sys_desc->fpu_model == FPU_MODEL_68882) {
        // MC68881/MC68882 User's Manual, page 5-10 (211)
        struct m68882_idle_frame* idle_p = (struct m68882_idle_frame*)vp->excpt_sa->fsave;

        if (idle_p->format == FSAVE_FORMAT_882_IDLE) {
            idle_p->biu_flags |= BIU_FP_EXCPT_PENDING;
        }
        return;
    }

    if (g_sys_desc->fpu_model == FPU_MODEL_68060) {
        // 68060UM, page 6-37
        struct m68060_fsave_frame* fsave_p = (struct m68060_fsave_frame*)vp->excpt_sa->fsave;

        if (fsave_p->format == FSAVE_FORMAT_68060_EXCP) {
            fsave_p->format = FSAVE_FORMAT_68060_IDLE;
        }
        return;
    }
}

// MC68060UM, p8-25 (257)
// XXX incomplete for now and just here to detect the case that the only
// problem is a branch prediction error (should RTE in this case instead of
// calling out to user space)
static bool recov_access_error_060(const excpt_frame_t* _Nonnull efp)
{
    const uint32_t fslw = efp->u.f4_access_error.fslw;

    // Step 3: transparent translation access error
    if ((fslw & FSLW_TTR) == FSLW_TTR) {
        return true;
    }


    // Step 4: invalid MMU description error
    if ((fslw & FSLW_TTR) == 0 && (fslw & (FSLW_TWE|FSLW_PTA|FSLW_PTB|FSLW_IL|FSLW_PF)) != 0) {
        return true;
    }


    // Step 5: MMU protection violation and bus error
    if ((fslw & (FSLW_SP|FSLW_WP|FSLW_RE|FSLW_WE)) != 0) {
        return true;
    }


    // Step 6: default transparent translation cases
    if ((fslw & (FSLW_TTR|FSLW_PTA|FSLW_PTB|FSLW_IL|FSLW_PF)) == 0) {
        return true;
    }


    // we know at this point that the access error was caused merely by a branch
    // prediction error. RTE back and do not invoke the user exception handler.
    return false;
}

// General exception information
// ------------------------------
// MC68020UM, p6-1 (126)ff
// MC68030UM, p9-1 (268)ff
// MC68040UM, p8-1 (220)ff, p9-20 (271)ff
// MC68060UM, p8-1 (233)ff
// MC68851UM, pC-1 (311)ff
// MC68881/MC68882 UM, p6-1 (218)ff
//
// VM/paging related information
// ------------------------------
// MC68020UM, p6-4 (129)ff, p6-22 (147); MC68851UM, pC-6 (316)ff, pC-21 (331) [context switch -> PSAVE/PRESTORE, bus error -> PTEST, 68851 style PTEs, 5 level PT, 0 TTRs]
// MC68030UM, p8-27 (294)ff, p9-1 (302)ff, p9-82 (383)ff [bus error -> PTEST, 68851 style PTEs, 5 level PT, 2 TTRs]
// MC68040UM, p8-24 (243)ff, p3-33 (84) [bus error -> PTEST, 68040 style PTEs, 3 level PT, 4 TTRs]
// MC68060UM, p8-5 (237), p8-21 (253)ff, p8-25 (257)ff, p4-1 (70)ff [bus error -> frame type $4, FSLW, 68040 style PTEs, 3 level PT, 4 TTRs]
//
// NOTES:
// - BUS ERROR: we do not attempt to repair bus errors in software for the following causes:
//  - unaligned data access
//  - physical RAM or I/O device does not exists
//  - I/O device does not allow access with certain data sizes
//
// - ADDRESS ERROR: we do not attempt to repair address errors in general (instructions have to be properly aligned)
// 
// - TRACE: currently nor supported. Keep in mind that:
//  - 68040 bus error handler has to invoke the trace handler in software in some cases. See MC68040UM, p8-25 (244)
//
// This handler returns:
// *) 1 if the assembler portion should continue to invoke the user space exception handler
// *) 0 if the assembler portion should immediately do a cpu_exception_return instead
int cpu_exception(struct vcpu* _Nonnull vp, excpt_0_frame_t* _Nonnull utp)
{
    void* ksp = ((char*)utp) + sizeof(excpt_0_frame_t);
    excpt_frame_t* efp = (excpt_frame_t*)&vp->excpt_sa->ef;
    const int ef_format = excpt_frame_getformat(efp);
    const int cpu_model = g_sys_desc->cpu_model;
    const int cpu_code = excpt_frame_getvecnum(efp);
    const bool is_f7_access_err = (cpu_model == CPU_MODEL_68040 && cpu_code == EXCPT_NUM_BUS_ERR && ef_format == 7);
    const bool is_f4_access_err = (cpu_model == CPU_MODEL_68060 && cpu_code == EXCPT_NUM_BUS_ERR && ef_format == 4);
    const excpt_handler_t* ehp = vcpu_get_excpt_handler_ref(vp);
    excpt_info_t ei;


    // Clear branch cache, in case of a branch prediction error
    if (is_f4_access_err && fslw_is_branch_pred_error(efp->u.f4_access_error.fslw)) {
        cpu_clear_branch_cache();
    }


    // get the exception code
    ei.code = get_ecode(cpu_model, cpu_code, efp);
    ei.cpu_code = cpu_code;

    // get the PC and fault address
    ei.pc = (void*)efp->pc;
    ei.addr = (void*)get_faddr(cpu_model, efp);


    // Halt system, if:
    // - exception triggered by supervisor (kernel)
    // - ecode == -1 which means that this is a fatal exception (e.g. faulty hardware)
    // - 68040, cache push physical bus error [MC68040UM, p8-31 (250)]
    // - 68060, push buffer bus error [MC68060UM, p8-25 (257)]
    // - 68060, store buffer bus error [MC68060UM, p8-25 (257)]
    if (!excpt_frame_isuser(efp)
        || ei.code < 0
        || (is_f7_access_err && ssw7_is_cache_push_phys_error(efp->u.f7.ssw))
        || (is_f4_access_err && fslw_is_push_buffer_error(efp->u.f4_access_error.fslw))
        || (is_f4_access_err && fslw_is_store_buffer_error(efp->u.f4_access_error.fslw))) {
        _fatalException(ksp, ei.addr);
        /* NOT REACHED */
    }

    // Terminate user process, if:
    // - nested exception
    // - no exception handler provided by user space
    // - 68060, an misaligned read-modify-write instruction [MC68060UM, p8-25 (257)]
    // - 68060, a move in which the destination op writes over its source op [MC68060UM, p8-25 (257)]
    if (vp->excpt_id > 0
        || (is_f4_access_err && fslw_is_misaligned_rmw(efp->u.f4_access_error.fslw))
        || (is_f4_access_err && fslw_is_self_overwriting_move(efp->u.f4_access_error.fslw))
        || ehp == NULL) {
        // double fault or no exception handler -> exit
        Process_Exit(vp->proc, JREASON_EXCEPTION, ei.code);
        /* NOT REACHED */
    }


    // FP fsave frame may require some fix up
    if (ei.code >= EXCPT_FLT_NAN && ei.code <= EXCPT_FLT_INEXACT) {
        fp_fsave_fixup(vp);
    }


    if (is_f4_access_err) {
        if (!recov_access_error_060(efp)) {
            return 0;
        }
    }


    // Record the active exception type
    vp->excpt_id = ei.code;


    // Push the exception info on the user stack
    struct u_excpt_frame* uep = (struct u_excpt_frame*)usp_grow(sizeof(struct u_excpt_frame));
    if ((vp->excpt_handler_flags & EXCPT_MCTX) == EXCPT_MCTX) {
        _vcpu_read_excpt_mcontext(vp, &uep->mc);
    }
    uep->ei = ei;
    uep->ei_ptr = &uep->ei;
    uep->mc_ptr = &uep->mc;
    uep->arg = ehp->arg;
    uep->ret_addr = (void*)excpt_return;


    // Update the u-trampoline with the exception function entry point
    utp->pc = (uintptr_t)ehp->func;

    return 1;
}

void cpu_exception_return(struct vcpu* _Nonnull vp, int excpt_hand_ret)
{
    if (_EXCPT_CACT(excpt_hand_ret) == EXCPT_CONTINUE_EXECUTION) {
        struct u_excpt_frame_ret* usp = (struct u_excpt_frame_ret*)usp_get();

        // Write back the (possibly) updated machine context
        if ((vp->excpt_handler_flags & EXCPT_MCTX) == EXCPT_MCTX
            && (_EXCPT_CFLAGS(excpt_hand_ret) & EXCPT_MODIFIED_MCTX) == EXCPT_MODIFIED_MCTX) {
            _vcpu_write_excpt_mcontext(vp, usp->mc_ptr);
        }

        // Pop the exception info off the user stack. Note that the return address
        // was already taken off by the CPU before we came here
        usp_shrink(sizeof(struct u_excpt_frame_ret));

        // This vcpu is no longer processing an exception
        vp->excpt_id = 0;
    }
    else {
        const int ecode = vp->excpt_id;
        
        vp->excpt_id = 0;
        Process_Exit(vp->proc, JREASON_EXCEPTION, ecode);
        /* NOT REACHED */
    }
}
