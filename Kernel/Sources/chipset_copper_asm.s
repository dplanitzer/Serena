;
;  chipset_copper_asm.s
;  Apollo
;
;  Created by Dietmar Planitzer on 2/2/21.
;  Copyright © 2021 Dietmar Planitzer. All rights reserved.
;

    include "chipset.i"
    include "lowmem.i"


    xref _gCopperSchedulerStorage

    xdef _copper_schedule_program
    xdef _copper_get_running_program_id
    xdef _copper_run
    xdef _copper_force_run_program


;-------------------------------------------------------------------------------
; void copper_schedule_program(const CopperInstruction* _Nullable pOddFieldProg, const CopperInstruction* _Nullable pEvenFieldProg, Int progId)
; Schedules the given odd and even field Copper programs for execution. The
; programs will start executing at the next vertical blank. Expects at least
; an odd field program if the current video mode is non-interlaced and both
; and odd and an even field program if the video mode is interlaced. The video
; display is turned off if the odd field program is NULL.
_copper_schedule_program:
    cargs   csp_odd_field_prog_ptr.l, csp_even_field_prog_ptr.l, csp_prog_id.l

    lea     _gCopperSchedulerStorage, a0
    DISABLE_INTERRUPTS  d1
    move.l  csp_prog_id(sp), d0
    move.l  d0, cop_scheduled_prog_id(a0)
    move.l  csp_odd_field_prog_ptr(sp), d0
    move.l  d0, cop_scheduled_prog_odd_field(a0)
    move.l  csp_even_field_prog_ptr(sp), d0
    move.l  d0, cop_scheduled_prog_even_field(a0)

    bset.b  #COPB_SCHEDULED, cop_flags(a0)
    RESTORE_INTERRUPTS  d1
    rts


;-------------------------------------------------------------------------------
; Int copper_get_running_program_id(void)
; Returns the program ID of the Copper program that is currently running. This is
; the program that is executing on every vertical blank.
_copper_get_running_program_id:
    move.l  _gCopperSchedulerStorage + cop_running_prog_id, d0
    rts


;-------------------------------------------------------------------------------
; void copper_run(void)
; Called at the vertical blank interrupt. Triggers the execution of the correct
; Copper program (odd or even field as needed). Also makes a scheduled program
; active / running if needed.
_copper_run:
    lea     _gCopperSchedulerStorage, a0
    lea     CUSTOM_BASE, a1

    ; check whether a new program is scheduled to run. If so move it to running
    ; state
    btst.b  #COPB_SCHEDULED, cop_flags(a0)
    beq.s   .cop_run_activate_prog

    ; move the scheduled program to running state. But be sure to first turn off
    ; the Copper and raster DMA. Then move the data. Then turn the Copper DMA
    ; back on if we have a prog. The program is responsible for turning the raster
    ; DMA on.
    move.w  #(DMAF_COPPER | DMAF_RASTER | DMAF_SPRITE), DMACON(a1)
    move.l  cop_scheduled_prog_id(a0), d0
    move.l  d0, cop_running_prog_id(a0)
    move.l  cop_scheduled_prog_odd_field(a0), d1
    move.l  d1, cop_running_prog_odd_field(a0)
    move.l  cop_scheduled_prog_even_field(a0), d0
    move.l  d0, cop_running_prog_even_field(a0)
    move.b  #0, cop_flags(a0)

    ; no odd field prog means that we should leave video turned off altogether
    tst.l   d1
    bne.s   .cop_run_has_progs
    rts

.cop_run_has_progs:
    tst.l   d0
    beq.s   .cop_run_activate_prog
    bset.b  #COPB_INTERLACED, cop_flags(a0)

.cop_run_activate_prog:
    ; activate the correct Copper program
    btst.b  #COPB_INTERLACED, cop_flags(a0)
    bne.s   .cop_run_interlaced

    ; handle non-interlaced (single field) programs
.cop_run_odd_field_prog:
    move.l  cop_running_prog_odd_field(a0), d0

.cop_run_done:
    move.l  d0, COP1LC(a1)
    move.w  d0, COPJMP1(a1)
    move.w  #(DMAF_SETCLR | DMAF_COPPER | DMAF_MASTER), DMACON(a1)
    rts

.cop_run_interlaced:
    ; handle interlaced (dual field) programs. Which program to activate depends
    ; on whether the current field is the even or the odd one
    move.w  VPOSR(a1), d0               ; set CCR.N if odd field (long frame)
    bpl.s   .cop_run_even_field_prog
    bra.s   .cop_run_odd_field_prog

.cop_run_even_field_prog:
    move.l  cop_running_prog_even_field(a0), d0
    bra.s   .cop_run_done


;-------------------------------------------------------------------------------
; void copper_force_run_program(const CopperInstruction* _Nullable pOddFieldProg)
; Forces the execution of the given Copper program. This is ment for debugging
; purposes only.
_copper_force_run_program:
    cargs cfrp_odd_field_prog.l

    move.l  cfrp_odd_field_prog(sp), d0
    lea     CUSTOM_BASE, a1

    ; move the scheduled program to running state. But be sure to first turn off
    ; the Copper and raster DMA. Then move the data. Then turn the Copper DMA
    ; back on if we have a prog. The program is responsible for turning the raster
    ; DMA on.
    move.w  #(DMAF_COPPER | DMAF_RASTER | DMAF_SPRITE), DMACON(a1)
    move.l  d0, COP1LC(a1)
    move.w  d0, COPJMP1(a1)
    move.w  #(DMAF_SETCLR | DMAF_COPPER | DMAF_MASTER), DMACON(a1)
    rts
