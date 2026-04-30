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

    excpt_info_t    ei;
};

// User exception frame layout after exiting the user exception handler.
struct u_excpt_frame_ret {
    void*           arg;
    excpt_info_t*   ei_ptr;

    excpt_info_t    ei;
};


// Used by cpu_asm.s
int8_t g_excpt_frame_size[16] = {8, 8, 12, 12, 12, 0, 0, 60, 0, 20, 32, 92, 0, 0, 0, 0};


static int get_ecode(int cpu_family, int cpu_code, excpt_frame_t* _Nonnull efp)
{
    switch (cpu_code) {
        case CPU_VEC_BUS_ERR:     // MC68040, MC68060: Access Fault
            if ((cpu_family == CPU_FAMILY_68040 && excpt_frame_getformat(efp) == 7 && (efp->u.f7.ssw & SSW7_MA) == SSW7_MA)
                || (cpu_family == CPU_FAMILY_68060 && excpt_frame_getformat(efp) == 4) && fslw_is_misaligned_rmw(efp->u.f4_access_error.fslw)) {
                return EXCPT_DATA_MISALIGNED;
            }
            else {
                return EXCPT_PAGE_ERROR;
            }

        case CPU_VEC_ADR_ERR:
            return EXCPT_INSTRUCTION_MISALIGNED;

        case CPU_VEC_ILLEGAL:
        case CPU_VEC_LINE_A:
        case CPU_VEC_PMMU_ACCESS:     // MC68851 PMMU is turned off and user space tries ot execute a PVALID instruction
        case CPU_VEC_UNIMPL_EA:       // MC68060  (TBD) -> 68060SP
        case CPU_VEC_UNIMPL_INST:     // MC68060  (TBD) -> 68060SP
        case CPU_VEC_EMU_INT:         // MC68060  (TBD) -> 68060SP
            return EXCPT_ILLEGAL_INSTRUCTION;

        case CPU_VEC_LINE_F:
            if (cpu_family < 68060 || (cpu_family >= CPU_FAMILY_68060 && excpt_frame_getformat(efp) == 4)) {
                // Either a < 68060 CPU with no FPU present (e.g. 68LC040 or 68030 with no 68881/68882 co-proc)
                // or a MC68060 class CPU with FPU disabled or a MC68LC060/MC68EC060 (no FPU)
                return EXCPT_ILLEGAL_INSTRUCTION;
            }
            else {
                // (TBD) if MC68040 then -> 68040FPSP; if MC68060 then -> 68060SP
                return EXCPT_ILLEGAL_INSTRUCTION;
            }

        case CPU_VEC_ZERO_DIV:
            return EXCPT_INT_DIVIDE_BY_ZERO;

        case CPU_VEC_PRIV_VIO:
            return EXCPT_PRIV_INSTRUCTION;

        case CPU_VEC_TRACE:
            return EXCPT_SINGLE_STEP;

        case CPU_VEC_CHK:
            return EXCPT_BOUNDS_EXCEEDED;

        case CPU_VEC_TRAP_0:
        case CPU_VEC_TRAP_1:
        case CPU_VEC_TRAP_2:
        case CPU_VEC_TRAP_4:
        case CPU_VEC_TRAP_5:
        case CPU_VEC_TRAP_6:
        case CPU_VEC_TRAP_7:
        case CPU_VEC_TRAP_8:
        case CPU_VEC_TRAP_9:
        case CPU_VEC_TRAP_10:
        case CPU_VEC_TRAP_11:
        case CPU_VEC_TRAP_12:
        case CPU_VEC_TRAP_13:
        case CPU_VEC_TRAP_14:
        case CPU_VEC_TRAP_15:
            return EXCPT_SOFT_INTERRUPT;

        case CPU_VEC_TRAP_3:
            return EXCPT_BREAKPOINT;

        case CPU_VEC_TRAPcc:      // MC68881, MC68882, MC68851
            if (excpt_frame_getformat(efp) == 2 && *((uint16_t*)efp->u.f2.addr) == 0xCE76 /*TRAPV*/) {
                return EXCPT_INT_OVERFLOW;
            }
            else {
                return EXCPT_SOFT_INTERRUPT;
            }

        case CPU_VEC_FPU_BRANCH_UO:
        case CPU_VEC_FPU_SNAN:
            return EXCPT_FLT_NAN;

        case CPU_VEC_FPU_INEXACT:
            return EXCPT_FLT_INEXACT;

        case CPU_VEC_FPU_DIV_ZERO:
            return EXCPT_FLT_DIVIDE_BY_ZERO;

        case CPU_VEC_FPU_UNDERFLOW:
            return EXCPT_FLT_UNDERFLOW;

        case CPU_VEC_FPU_OP_ERR:
        case CPU_VEC_FPU_UNIMPL_TY:   // MC68040
            return EXCPT_FLT_OPERAND;

        case CPU_VEC_FPU_OVERFLOW:
            return EXCPT_FLT_OVERFLOW;

        case CPU_VEC_UNINIT_IRQ:
        case CPU_VEC_SPUR_IRQ:
        case CPU_VEC_IRQ_7:           // NMI
        case CPU_VEC_COPROC:          // MC68881, MC68882, MC68851
        case CPU_VEC_FORMAT:
        case CPU_VEC_MMU_CONFIG:      // MC68030 MMU, MC68851 PMMU
        case CPU_VEC_PMMU_ILLEGAL:    // MC68851 PMMU
            return -1;

        case CPU_VEC_USER_VEC + 0:
            return (!excpt_frame_ishw(efp)) ? EXCPT_FORCED_ABORT : -1;

        case CPU_VEC_USER_VEC + 1:
            return (!excpt_frame_ishw(efp)) ? EXCPT_USER : -1;

        case CPU_VEC_USER_VEC + 2:
            return (!excpt_frame_ishw(efp)) ? EXCPT_USER_2 : -1;

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

static uintptr_t get_fault_addr(int cpu_family, const excpt_frame_t* _Nonnull efp)
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
            if (cpu_family == CPU_FAMILY_68040) {
                // MC68LC040 (no FPU)
                // MC68EC040 (no FPU, no MMU)
                // We return the PC of the faulted FP instruction to align us with
                // the standard illegal instruction exception type
                return efp->u.f4_line_f.pcFaultedInstr;
            }
            else if (cpu_family >= CPU_FAMILY_68060) {
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

        default:
            return 0;
    }
}

static void fp_fsave_fixup(struct vcpu* _Nonnull vp)
{
    const int fpu_type = cpu_68k_fpu(g_sys_desc->cpu_subtype);

    if (fpu_type == CPU_FPU_68882) {
        // MC68881/MC68882 User's Manual, page 5-10 (211)
        struct m68882_idle_frame* idle_p = (struct m68882_idle_frame*)vp->excpt_sa->f.fsave;

        if (idle_p->format == FSAVE_FORMAT_882_IDLE) {
            idle_p->biu_flags |= BIU_FP_EXCPT_PENDING;
        }
        return;
    }

    if (fpu_type == CPU_FPU_68060) {
        // 68060UM, page 6-37
        struct m68060_fsave_frame* fsave_p = (struct m68060_fsave_frame*)vp->excpt_sa->f.fsave;

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
    excpt_frame_t* efp = (excpt_frame_t*)&vp->excpt_sa->b.ef;
    const bool is_hw_excpt = excpt_frame_ishw(efp);
    const int ef_format = excpt_frame_getformat(efp);
    const int cpu_family = cpu_68k_family(g_sys_desc->cpu_subtype);
    const int cpu_code = (is_hw_excpt) ? excpt_frame_getvecnum(efp) : excpt_frame_getvecnum(efp) - CPU_VEC_SOFT_EXCPT;
    const bool is_f7_access_err = (cpu_family == CPU_FAMILY_68040 && cpu_code == CPU_VEC_BUS_ERR && ef_format == 7);
    const bool is_f4_access_err = (cpu_family == CPU_FAMILY_68060 && cpu_code == CPU_VEC_BUS_ERR && ef_format == 4);
    const bool is_nested = vcpu_is_handling_exception(vp);


    // Clear branch cache, in case of a branch prediction error
    if (is_f4_access_err && fslw_is_branch_pred_error(efp->u.f4_access_error.fslw)) {
        cpu_clear_branch_cache();
    }


    // Figure out which exception handler function to call
    excpt_handler_t eh = vp->excpt_handler;
    if (eh.func == NULL) {
        Process_GetExceptionHandler(vp->proc, &eh);
    }


    // Get the exception code
    vp->excpt_state.code = get_ecode(cpu_family, cpu_code, efp);
    vp->excpt_state.cpu_code = cpu_code;

    // Get the PC and fault address
    vp->excpt_state.sp = (void*)vp->excpt_sa->b.usp;
    vp->excpt_state.pc = (void*)efp->pc;
    vp->excpt_state.fault_addr = (void*)get_fault_addr(cpu_family, efp);


    // Halt system, if:
    // - exception triggered by supervisor (kernel)
    // - ecode == -1 which means that this is a fatal exception (e.g. faulty hardware)
    // - 68040, cache push physical bus error [MC68040UM, p8-31 (250)]
    // - 68060, push buffer bus error [MC68060UM, p8-25 (257)]
    // - 68060, store buffer bus error [MC68060UM, p8-25 (257)]
    if (!excpt_frame_isuser(efp)
        || vp->excpt_state.code < 0
        || (is_f7_access_err && ssw7_is_cache_push_phys_error(efp->u.f7.ssw))
        || (is_f4_access_err && fslw_is_push_buffer_error(efp->u.f4_access_error.fslw))
        || (is_f4_access_err && fslw_is_store_buffer_error(efp->u.f4_access_error.fslw))) {
        void* ksp = ((char*)utp) + sizeof(excpt_0_frame_t);
        
        _fatalException(ksp);
        /* NOT REACHED */
    }

    // Terminate user process, if:
    // - nested exception
    // - no exception handler provided by user space
    // - 68060, an misaligned read-modify-write instruction [MC68060UM, p8-25 (257)]
    // - 68060, a move in which the destination op writes over its source op [MC68060UM, p8-25 (257)]
    if (is_nested
        || (is_f4_access_err && fslw_is_misaligned_rmw(efp->u.f4_access_error.fslw))
        || (is_f4_access_err && fslw_is_self_overwriting_move(efp->u.f4_access_error.fslw))
        || eh.func == NULL) {
        // double fault or no exception handler -> exit
        Process_Exit(vp->proc, STATUS_REASON_EXCEPTION, vp->excpt_state.code);
        /* NOT REACHED */
    }


    if (is_hw_excpt) {
        // FP fsave frame may require some fix up
        if (vp->excpt_state.code >= EXCPT_FLT_NAN && vp->excpt_state.code <= EXCPT_FLT_INEXACT) {
            fp_fsave_fixup(vp);
        }


        if (is_f4_access_err) {
            if (!recov_access_error_060(efp)) {
                return 0;
            }
        }
    }


    // Push the exception info on the user stack
    struct u_excpt_frame* uep = (struct u_excpt_frame*)usp_grow(sizeof(struct u_excpt_frame));
    uep->ei.code = vp->excpt_state.code;
    uep->ei.cpu_code = vp->excpt_state.cpu_code;
    uep->ei.sp = vp->excpt_state.sp;
    uep->ei.pc = vp->excpt_state.pc;
    uep->ei.fault_addr = vp->excpt_state.fault_addr;
    uep->ei_ptr = &uep->ei;
    uep->arg = eh.arg;
    uep->ret_addr = (void*)_excpt_return;


    // Update the u-trampoline with the exception function entry point
    utp->pc = (uintptr_t)eh.func;

    return 1;
}

void cpu_exception_return(struct vcpu* _Nonnull vp, int excpt_hand_ret)
{
    const int ecode = vp->excpt_state.code;

    // This vcpu is no longer processing an exception
    vp->excpt_state.code = 0;


    if (excpt_hand_ret != EXCPT_CONTINUE_EXECUTION) {
        Process_Exit(vp->proc, STATUS_REASON_EXCEPTION, ecode);
        /* NOT REACHED */
    }


    uintptr_t orig_excpt_pc = vp->excpt_sa->b.ef.pc;
    struct u_excpt_frame_ret* usp = (struct u_excpt_frame_ret*)usp_get();

    // Pop the exception info off the user stack. Note that the return address
    // was already taken off by the CPU before we came here
    usp_shrink(sizeof(struct u_excpt_frame_ret));


    // Check whether the user program has changed the PC at which we should continue.
    // If it has then we need to replace the original exception frame with a type
    // $0 frame to ensure that the CPU won't restart the originally interrupted
    // instruction. E.g. page faults generate a type $4 or $7 frame which, if we
    // would RTE that frame type, would cause the CPU to restart the interrupted
    // instruction. We don't want that so we dismiss the existing exception frame
    // and we replace it with a simple $0 frame.
    if (vp->excpt_sa->b.ef.pc != orig_excpt_pc && excpt_frame_getformat(&vp->excpt_sa->b.ef) > 1) {
        const int8_t frm_siz = g_excpt_frame_size[excpt_frame_getformat(&vp->excpt_sa->b.ef)];
        const uint32_t* src_p = (const uint32_t*)&vp->excpt_sa->b.ef;
        uint32_t* dst_p = (uint32_t*)((char*)src_p - frm_siz + 8);

        // We want to dismiss the original exception frame and replace it with a
        // type $0 frame:
        // - copy the type $0 portion of the original frame up to size(original frame) - 8 bytes
        // - replace the pc field in the original exception frame $0 portion with the number of bytes to pop from the stack
        // - set the lsb in the original frame version word to 1 to mark this frame as special
        // Note that a M68K CPU will never set the lsb in the frame version word.
        dst_p[0] = src_p[0];
        dst_p[1] = src_p[1];

        vp->excpt_sa->b.ef.pc = frm_siz - 8;
        vp->excpt_sa->b.ef.fv |= 0x01;
    }
}
