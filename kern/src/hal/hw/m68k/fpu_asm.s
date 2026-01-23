;
;  machine/hw/m68k/cpu_asm.s
;  kernel
;
;  Created by Dietmar Planitzer on 2/4/21.
;  Copyright © 2021 Dietmar Planitzer. All rights reserved.
;

    include "lowmem.i"

    xdef _fpu_get_model
    xdef _fpu_idle_fsave



;-------------------------------------------------------------------------------
; int fpu_get_model(void)
; Returns the FPU model identifier
_fpu_get_model:
    inline
        movem.l a2 / d2, -(sp)
        move.l  #44, a0     ; install the line F handler
        move.l  (a0), -(sp)
        lea     .done(pc), a1
        move.l  a1, (a0)
        move.l  sp, a2     ; save the stack pointer

        moveq   #FPU_MODEL_NONE, d0
        dc.l    $f201583a  ; ftst.b d1 - force the fsave instruction to generate an IDLE frame
        dc.w    $f327      ; fsave -(a7) - save the IDLE frame to the stack
        move.l  a2, d2
        sub.l   sp, d2     ; find out how big the IDLE frame is
        moveq   #FPU_MODEL_68881, d0
        cmp.b   #$1c, d2   ; 68881 IDLE frame size
        beq.s   .done
        moveq   #FPU_MODEL_68882, d0
        cmp.b   #$3c, d2   ; 68882 IDLE frame size
        beq.s   .done
        moveq   #FPU_MODEL_68040, d0
        cmp.b   #$4, d2     ; 68040 IDLE frame size
        beq.s   .done
        moveq   #FPU_MODEL_68060, d0
        cmp.b   #$c, d2
        beq.s   .done
        moveq   #FPU_MODEL_NONE, d0 ; unknown FPU model

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
