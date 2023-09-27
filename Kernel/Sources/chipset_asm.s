;
;  chipset_asm.s
;  Apollo
;
;  Created by Dietmar Planitzer on 2/2/21.
;  Copyright © 2021 Dietmar Planitzer. All rights reserved.
;

    include "chipset.i"
    include "lowmem.i"


    xref _gCopperSchedulerStorage

    xdef _chipset_reset
    xdef _chipset_enable_interrupt
    xdef _chipset_disable_interrupt
    xdef _chipset_start_quantum_timer
    xdef _chipset_stop_quantum_timer
    xdef _chipset_get_quantum_timer_duration_ns
    xdef _chipset_get_quantum_timer_elapsed_ns


;-------------------------------------------------------------------------------
; void chipset_reset(void)
; Disables all chipset interrupts and DMAs. This function is called before we
; jump into the kernel proper.
_chipset_reset:
    lea     CUSTOM_BASE, a0
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
    move.b  #0, CIAAICR
    move.b  #0, CIABICR

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

    ; set all color registers to black
    moveq.l #0, d0
    move.w  #COLOR_REGS_COUNT-1, d1
.L1:
    move.w  d0, COLOR_BASE(a0, d1.w*2)
    subq.w  #1, d1
    cmp.w   #0, d1
    bge.w   .L1

    rts


;-------------------------------------------------------------------------------
; void chipset_enable_interrupt(Int interruptId)
; Enables generation of the given interrupt type. This implicitly turns on the
; master IRQ switch.
; Note that we always turn on the master IRQ switch, external IRQs and the ports
; IRQs because it makes things simpler. External & ports IRQs are masked in the
; chips that are the source of an IRQ. CIA A/B chips.
_chipset_enable_interrupt:
    cargs cei_interrupt_id.l
    move.l  cei_interrupt_id(sp), d0

    moveq.l #0, d1
    bset    #0, d1

    ; handle chipset interrupts
    cmp.b   #IRQ_SOURCE_EXTERN, d0
    bgt.s   cei_ciaa_interrupts
    lsl.w   d0, d1
    or.w    #(INTF_SETCLR | INTF_INTEN | INTF_EXTER | INTF_PORTS), d1
    move.w  d1, CUSTOM_BASE + INTENA
    bra.s   cei_done

cei_ciaa_interrupts:
    ; handle CIA A interrupts
    cmp.b   #IRQ_SOURCE_CIA_A_FLAG, d0
    bgt.s   cei_ciab_interrupts
    sub.b   #IRQ_SOURCE_CIA_A_TIMER_A, d0
    lsl.b   d0, d1
    bset    #ICRB_IR, d1
    move.b  d1, CIAAICR
    bra.s   cei_ciax_inten

cei_ciab_interrupts:
    ; handle CIA B interrupts
    cmp.b   #IRQ_SOURCE_CIA_B_FLAG, d0
    bgt.s   cei_done
    sub.b   #IRQ_SOURCE_CIA_B_TIMER_A, d0
    lsl.b   d0, d1
    bset    #ICRB_IR, d1
    move.b  d1, CIABICR
    ; fall through

cei_ciax_inten:
    ; we enabled one of the CIA IRQs. We also need to turn on the master IRQ switch
    move.w #(INTF_SETCLR | INTF_INTEN | INTF_EXTER | INTF_PORTS), d1
    move.w  d1, CUSTOM_BASE + INTENA
    ; fall through

cei_done:
    rts


;-------------------------------------------------------------------------------
; void chipset_disable_interrupt(Int interruptId)
; Disables the generation of the given interrupt type. This function leaves the
; master IRQ switch enabled. It doesn't matter 'cause we turn the IRQ source off
; anyway.
_chipset_disable_interrupt:
    cargs cdi_interrupt_id.l
    move.l  cdi_interrupt_id(sp), d0

    moveq.l #0, d1
    bset    #0, d1

    ; handle chipset interrupts
    cmp.b   #IRQ_SOURCE_EXTERN, d0
    bgt.s   cdi_ciaa_interrupts
    lsl.w   d0, d1
    bclr    #INTB_SETCLR, d1
    move.w  d1, CUSTOM_BASE+ INTENA
    bra.s   cdi_done

cdi_ciaa_interrupts:
    ; handle CIA A interrupts
    cmp.b   #IRQ_SOURCE_CIA_A_FLAG, d0
    bgt.s   cdi_ciab_interrupts
    sub.b   #IRQ_SOURCE_CIA_A_TIMER_A, d0
    lsl.b   d0, d1
    bclr    #ICRB_IR, d1
    move.b  d1, CIAAICR
    bra.s   cdi_done

cdi_ciab_interrupts:
    ; handle CIA B interrupts
    cmp.b   #IRQ_SOURCE_CIA_B_FLAG, d0
    bgt.s   cdi_done
    sub.b   #IRQ_SOURCE_CIA_B_TIMER_A, d0
    lsl.b   d0, d1
    bclr    #ICRB_IR, d1
    move.b  d1, CIABICR
    ; fall through

cdi_done:
    rts


;-------------------------------------------------------------------------------
; void chipset_start_quantum_timer(void)
; Starts the quantum timer running. This timer is used to implement context switching.
; Uses timer B in CIA A.
;
; Amiga system clock:
;   NTSC    28.63636 MHz
;   PAL     28.37516 MHz
;
; CIA A timer B clock:
;   NTSC    0.715909 MHz (1/10th CPU clock)     [1.3968255 us]
;   PAL     0.709379 MHz                        [1.4096836 us]
;
; Quantum duration:
;   NTSC    16.761906 ms    [12000 timer clock cycles]
;   PAL     17.621045 ms    [12500 timer clock cycles]
;
; The quantum duration is chosen such that:
; - it is approx 16ms - 17ms
; - the value is a positive integer in terms of nanoseconds to avoid accumulating / rounding errors as time progresses
;
_chipset_start_quantum_timer:
    ; stop the timer
    jsr     _chipset_stop_quantum_timer

    ; load the timer with the new ticks value
    move.w  SYS_DESC_BASE + sd_quantum_duration_cycles, d0
    move.b  d0, CIAATBLO
    lsr.w   #8, d0
    move.b  d0, CIAATBHI

    ; start the timer
    move.b  CIAACRB, d0
    or.b    #%00010001, d0
    and.b   #%10010001, d0
    move.b  d0, CIAACRB

    rts


;-------------------------------------------------------------------------------
; void chipset_stop_quantum_timer(void)
; Stops the quantum timer.
_chipset_stop_quantum_timer:
    move.b  CIAACRB, d1
    and.b   #%11101110, d1
    move.b  d1, CIAACRB
    rts


;-------------------------------------------------------------------------------
; Int32 chipset_get_quantum_timer_duration_ns(void)
; Returns the length of a quantum in terms of nanoseconds.
_chipset_get_quantum_timer_duration_ns:
    move.l  SYS_DESC_BASE + sd_quantum_duration_ns, d0
    rts


;-------------------------------------------------------------------------------
; Int32 chipset_get_quantum_timer_elapsed_ns(void)
; Returns the amount of nanoseconds that have elapsed in the current quantum.
_chipset_get_quantum_timer_elapsed_ns:
    ; read the current timer value
    moveq.l #0, d1
    move.b  CIAATBHI, d1
    asl.w   #8, d1
    move.b  CIAATBLO, d1

    ; elapsed_ns = (quantum_duration_cycles - current_cycles) * ns_per_cycle
    move.w  SYS_DESC_BASE + sd_quantum_duration_cycles, d0
    sub.w   d1, d0
    muls    SYS_DESC_BASE + sd_ns_per_quantum_timer_cycle, d0
    rts
