;
;  machine/hw/m68k/cpu_asm.s
;  kernel
;
;  Created by Dietmar Planitzer on 2/4/21.
;  Copyright © 2021 Dietmar Planitzer. All rights reserved.
;

    include "cpu.i"

    xdef _cpu_probe_fpu
    xdef _fpu_idle_fsave
    xdef __cpu_get_float_regs
    xdef __cpu_set_float_regs



;-------------------------------------------------------------------------------
; int cpu_probe_fpu(void)
; Returns the FPU model identifier
_cpu_probe_fpu:
    inline
        movem.l a2 / d2, -(sp)
        move.l  #44, a0     ; install the line F-line handler
        move.l  (a0), -(sp)
        lea     .no_fpu(pc), a1
        move.l  a1, (a0)
        move.l  sp, a2     ; save the stack pointer

        dc.l    $f201583a  ; ftst.b d1 - force the fsave instruction to generate an IDLE frame
        dc.w    $f327      ; fsave -(a7) - save the IDLE frame to the stack
        move.l  a2, d2
        sub.l   sp, d2     ; find out how big the IDLE frame is
        moveq   #CPU_FPU_68881, d0
        cmp.b   #$1c, d2   ; 68881 IDLE frame size
        beq.s   .done
        moveq   #CPU_FPU_68882, d0
        cmp.b   #$3c, d2   ; 68882 IDLE frame size
        beq.s   .done
        moveq   #CPU_FPU_68040, d0
        cmp.b   #$4, d2     ; 68040 IDLE frame size
        beq.s   .done
        moveq   #CPU_FPU_68060, d0
        cmp.b   #$c, d2
        beq.s   .done

.no_fpu:
        moveq   #CPU_FPU_NONE, d0 ; unknown or no FPU

.done:
        move.l  a2, sp      ; restore the stack pointer
        move.l  (sp)+, (a0) ; restore the line F handler
        movem.l (sp)+, a2 / d2
        rts
    einline


;-------------------------------------------------------------------------------
; void fpu_idle_fsave(char* _Nonnull fsave)
; Fills the provided buffer with an idle fsave frame
_fpu_idle_fsave:
    inline
    cargs   fifs_fsave_ptr.l

        fmove   #0, fp0
        fadd    #0, fp0
        move.l  fifs_fsave_ptr(sp), a0

        ; this will write an idle frame since no exception is pending and we've
        ; executed at least one FP instruction
        fsave   (a0)
        rts
    einline


;-------------------------------------------------------------------------------
; void _cpu_get_float_regs(vcpu_state_m68k_float_t* _Nonnull dp)
; keep in sync with structure vcpu_state_m68k_float
__cpu_get_float_regs:
    cargs   fgfr_dp.l

    move.l      fgfr_dp(sp), a0
    fmovem.l    fpiar, (a0)+
    fmovem.l    fpsr, (a0)+
    fmovem.l    fpcr, (a0)+
    fmovem      fp0 - fp7, (a0)
    rts


;-------------------------------------------------------------------------------
; void _cpu_set_float_regs(const vcpu_state_m68k_float_t* _Nonnull dp)
; keep in sync with structure vcpu_state_m68k_float
__cpu_set_float_regs:
    cargs   fsfr_dp.l

    move.l      fsfr_dp(sp), a0
    fmovem.l    (a0)+, fpiar
    fmovem.l    (a0)+, fpsr
    fmovem.l    (a0)+, fpcr
    fmovem      (a0), fp0 - fp7
    rts
