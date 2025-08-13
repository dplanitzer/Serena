;
;  machine/amiga/chipset.s
;  kernel
;
;  Created by Dietmar Planitzer on 2/2/21.
;  Copyright © 2021 Dietmar Planitzer. All rights reserved.
;

    include "chipset.i"
    include <machine/lowmem.i>


    xdef _chipset_reset


;-------------------------------------------------------------------------------
; void chipset_reset(void)
; Disables all chipset interrupts and DMAs. This function is called before we
; jump into the kernel proper.
_chipset_reset:
    lea     CUSTOM_BASE, a0

    ; set all color registers to black
    moveq.l #0, d0
    move.w  #COLOR_REGS_COUNT-1, d1
.L1:
    move.w  d0, COLOR_BASE(a0, d1.w*2)
    subq.w  #1, d1
    cmp.w   #0, d1
    bge.w   .L1

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

    ; turn the motors of all floppy drives off and make sure we leave them in a deselected state
    move.b  CIABPRB, d0         ; deselect all drives and the disk step bit
    or.b    #%01111001, d0
    move.b  d0, CIABPRB
    bset    #7, d0              ; turn off motor and select all drives. The drives will latch the new motor state
    and.b   #%10000111, d0
    move.b  d0, CIABPRB
    or.b    #%01111000, d0      ; and finally deselect all drives for good
    move.b  d0, CIABPRB

    rts
