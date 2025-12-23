;
;  machine/hw/m68k-amiga/reset_asm.s
;  kernel
;
;  Created by Dietmar Planitzer on 2/2/21.
;  Copyright © 2021 Dietmar Planitzer. All rights reserved.
;


    include "chipset.i"
    include <hal/hw/m68k/lowmem.i>

    xref _OnReset
    xref _OnBoot
    xref _cpu_get_model
    xref _sys_desc_init
    xref _cpu_vector_table

    xdef _Reset
    xdef _cpu_non_recoverable_error

;-------------------------------------------------------------------------------
; Boot ROM Reset entry point
    align 2
_Reset:
    inline
        ; boot ROM off; power light dimmed
        move.b  #3, CIAADDRA
        move.b  #2, CIAAPRA

        ; make sure that all DMAs and IRQs are off
        jsr     _chipset_reset

        ; install the CPU vector table
        lea     _cpu_vector_table(pc), a0
        move.w  #(CPU_VECTORS_SIZEOF/4)-1, d0
        lea     CPU_VECTORS_BASE, a1
.L1:    move.l  (a0)+, (a1)+
        dbra    d0, .L1

        ; clear the system description
        lea     SYS_DESC_BASE, a0
        move.w  #(SYS_DESC_SIZEOF/4)-1, d0
        moveq.l #0, d1
.L2:    move.l  d1, (a0)+
        dbra    d0, .L2

        ; figure out which type of CPU this is. The required minimum is:
        ; CPU: MC68020
        ; FPU: none
        jsr     _cpu_get_model
        cmp.b   #CPU_MODEL_68020, d0
        bge     .L3
        move.l  #RGB_YELLOW, -(sp)
        jmp     _cpu_non_recoverable_error
.L3:

        ; Initialize the system description
        move.l  d0, -(sp)
        move.l  #BOOT_SERVICES_MEM_TOP, -(sp)
        pea     SYS_DESC_BASE
        jsr     _sys_desc_init
        add.l   #12, sp

        ; call the OnBoot(sys_desc_t*) routine
        pea     SYS_DESC_BASE
        jsr     _OnBoot

        ; NOT REACHED
        move.l  #RGB_YELLOW, -(sp)
        jmp     _cpu_non_recoverable_error
    einline


;-------------------------------------------------------------------------------
; void chipset_reset(void)
; Disables all chipset interrupts and DMAs. This function is called before we
; jump into the kernel proper.
_chipset_reset:
    lea     CUSTOM_BASE, a0

    ; set all color registers to white
    lea COLOR00(a0), a1
    move.w #$0fff, d0
    move.w  #COLOR_REGS_COUNT-1, d1
.L1:
    move.w  d0, (a1)+
    dbra    d1, .L1

    move.w  #$7fff, INTENA(a0)  ; disable interrupts
    move.w  #$7fff, INTREQ(a0)  ; clear pending interrupts
    move.w  #$7fff, DMACON(a0)  ; disable DMA
    move.w  #$4000, DSKLEN(a0)  ; disable disk DMA
    move.w  #$ff00, POTGO(a0)   ; configure the Paula bi-directional I/O port

    ; disable timers
    move.b  #0, CIAACRA
    move.b  #0, CIAACRB
    move.b  #0, CIABCRA
    move.b  #0, CIABCRB
    move.b  #$7f, CIAAICR
    move.b  #$7f, CIABICR

    ; clear pending IRQs
    move.b  CIAAICR, d0
    move.b  CIABICR, d0

    ; configure the CIA chips port direction registers
    move.b  #$03, CIAADDRA
    move.b  #$ff, CIAADDRB
    move.b  #$ff, CIABDDRA
    move.b  #$ff, CIABDDRB

    ; turn the motor of all floppy drives off and make sure we leave them in a deselected state
    move.b  CIABPRB, d0         ; deselect all drives and the disk step bit
    or.b    #%01111001, d0
    move.b  d0, CIABPRB
    bset    #7, d0              ; turn off motor and select all drives. The drives will latch the new motor state
    and.b   #%10000111, d0
    move.b  d0, CIABPRB
    or.b    #%01111000, d0      ; and finally deselect all drives for good
    move.b  d0, CIABPRB

    rts


;-------------------------------------------------------------------------------
; Invoked if we encountered a non-recoverable CPU error. Eg the CPU type isn't
; supported. Sets the screen color to yellow and halts the machine
    align 2
_cpu_non_recoverable_error:
    inline
    cargs   cnre_rgb_color.l

        move.l  cnre_rgb_color(sp), d0
        lea     CUSTOM_BASE, a0
        move.w  #$7fff, DMACON(a0)
        move.w  d0, COLOR00(a0)
        jmp     _cpu_halt
        ; NOT REACHED
    einline
