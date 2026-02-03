;
;  machine/hw/m68k/cpu.i
;  kernel
;
;  Created by Dietmar Planitzer on 2/2/21.
;  Copyright © 2021 Dietmar Planitzer. All rights reserved.
;

        ifnd CPU_I
CPU_I    set 1


; CPU models
CPU_MODEL_68000     equ 0
CPU_MODEL_68010     equ 1
CPU_MODEL_68020     equ 2
CPU_MODEL_68030     equ 3
CPU_MODEL_68040     equ 4
CPU_MODEL_68060     equ 6

CPU_STATE_SIZE  equ 4 + 8*4 + 7*4


; FPU models
FPU_MODEL_NONE      equ 0
FPU_MODEL_68881     equ 1
FPU_MODEL_68882     equ 2
FPU_MODEL_68040     equ 3
FPU_MODEL_68060     equ 4

FPU_USER_STATE_SIZE equ 108
FPU_MAX_FSAVE_SIZE  equ 216


; 68060 CACR
CACR_EDC_BIT        equ 31
CACR_NAD_BIT        equ 30
CACR_ESP_BIT        equ 29
CACR_DPI_BIT        equ 28
CACR_FOC_BIT        equ 27
CACR_EBC_BIT        equ 23
CACR_CABC_BIT       equ 22
CACR_CUBC_BIT       equ 21
CACR_EIC_BIT        equ 15
CACR_NAI_BIT        equ 14
CACR_FIC_BIT        equ 13


;-------------------------------------------------------------------------------
; Macros to enable / disable preemption by the virtual processor scheduler
;
; Usage:
;
; DISABLE_PREEMPTION d7
; < do stuff>
; RESTORE_PREEMPTION d7
;

; Disable context switches triggered by preemption (IRQs and quantum interrupts).
; This will stop IRQs from being serviced which turns off scheduling and context
; switching in interrupt contexts until they are reenabled.
; Returns the previous state of the status register.
    macro DISABLE_PREEMPTION
    move.w  sr, \1
    or.w    #$0700, sr
    endm

; Sets the current preemption state based on the saved state in a register
    macro RESTORE_PREEMPTION
    move.w  \1, sr
    endm


;-------------------------------------------------------------------------------
; Macros to enable / disable interrupt handling by the interrupt controller
;
; Usage:
;
; DISABLE_INTERRUPTS d7
; < do stuff>
; RESTORE_INTERRUPTS d7
;
; NOTE: These macros are functionality-wise a super set of the DISABLE_PREEMPTION
; and RESTORE_PREEMPTION macros because they disable ALL interrupts including
; those which do not cause preemptions. The preemption macros only disable
; interrupts which may trigger a preemption.

; Disable interrupt handling.
; Returns the previous state of the status register.
    macro DISABLE_INTERRUPTS
    move.w  sr, \1
    or.w    #$0700, sr
    endm

; Sets the current interrupt handling state based on the saved state in a register
    macro RESTORE_INTERRUPTS
    move.w  \1, sr
    endm

; Disable interrupt handling and saves the previous SR on the stack.
    macro DISABLE_INTERRUPTS_SP
    move.w  sr, -(sp)
    or.w    #$0700, sr
    endm

; Sets the current interrupt handling state based on the saved SR from the stack
    macro RESTORE_INTERRUPTS_SP
    move.w  (sp)+, sr
    endm



; Macro to store a pointer to the currently running virtual processor in the
; provided register
    macro GET_CURRENT_VP
    move.l  _g_sched, \1
    move.l  sched_running(\1), \1
    endm


;-------------------------------------------------------------------------------
; Context save/restore macros
;

    macro SAVE_CPU_STATE
    movem.l d0 - d7 / a0 - a6, -(sp)
    move.l  usp, a1         ; save the user stack pointer
    move.l  a1, -(sp)
    endm


    macro RESTORE_CPU_STATE
    move.l  (sp)+, a1       ; restore the user stack pointer
    move.l  a1, usp
    movem.l (sp)+, d0 - d7 / a0 - a6
    endm


    ; Saves the FPU state. Takes a register which stores a pointer to the vcpu
    ; whose's state should be saved.
    macro SAVE_FPU_STATE \1
    btst    #VP_FLAG_HAS_FPU_BIT, vp_flags(\1)
    bne.s   .sav_fpu_1\@
    sub.l   #(FPU_MAX_FSAVE_SIZE + FPU_USER_STATE_SIZE), sp
    bra.s   .sav_fpu_3\@

    ; save the FPU state. Note that the 68060 fmovem.l instruction does not
    ; support moving > 1 register at a time
.sav_fpu_1\@:
    sub.l       #FPU_MAX_FSAVE_SIZE, sp
    fsave       (sp)
    tst.b       (sp)
    bne.s       .sav_fpu_2\@
    sub.l       #FPU_USER_STATE_SIZE, sp
    bra.s       .sav_fpu_3\@
.sav_fpu_2\@:
    fmovem      fp0 - fp7, -(sp)
    fmovem.l    fpcr, -(sp)
    fmovem.l    fpsr, -(sp)
    fmovem.l    fpiar, -(sp)

.sav_fpu_3\@:
    endm


    ; Saves the FPU state. Takes a register which stores a pointer to the vcpu
    ; whose's state should be restored.
    macro RESTORE_FPU_STATE \1
    ; check whether we should restore the FPU state
    btst    #VP_FLAG_HAS_FPU_BIT, vp_flags(\1)
    bne.s   .rest_fpu_4\@
    add.l   #(FPU_MAX_FSAVE_SIZE + FPU_USER_STATE_SIZE), sp
    bra.s   .rest_fpu_7\@

    ; restore the FPU state. Note that the 68060 fmovem.l instruction does not
    ; support moving > 1 register at a time
.rest_fpu_4\@:
    tst.b       FPU_USER_STATE_SIZE(sp)
    bne.s       .rest_fpu_5\@
    add.l       #FPU_USER_STATE_SIZE, sp
    bra.s       .rest_fpu_6\@
.rest_fpu_5\@:
    fmovem.l    (sp)+, fpiar
    fmovem.l    (sp)+, fpsr
    fmovem.l    (sp)+, fpcr
    fmovem      (sp)+, fp0 - fp7

.rest_fpu_6\@:
    frestore    (sp)
    add.l       #FPU_MAX_FSAVE_SIZE, sp

.rest_fpu_7\@:
    endm
    
        endif ; CPU_I
