;
;  machine/amiga/clock.s
;  kernel
;
;  Created by Dietmar Planitzer on 2/2/21.
;  Copyright © 2021 Dietmar Planitzer. All rights reserved.
;

    include "chipset.i"
    include <machine/lowmem.i>

    xref _gMonotonicClockStorage


    xdef _mclk_start_quantum_timer
    xdef _mclk_stop_quantum_timer
    xdef _mclk_get_quantum_timer_elapsed_ns
    xdef _MonotonicClock_GetCurrentQuantums


;-------------------------------------------------------------------------------
; void mclk_start_quantum_timer(const MonotonicClock* _Nonnull self)
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
_mclk_start_quantum_timer:
    cargs csqt_clock_ptr.l

    move.l  csqt_clock_ptr(sp), a0

    ; stop the timer
    jsr     _mclk_stop_quantum_timer

    ; load the timer with the new ticks value
    move.w  mtc_quantum_duration_cycles(a0), d0
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
; void mclk_stop_quantum_timer(void)
; Stops the quantum timer.
_mclk_stop_quantum_timer:
    move.b  CIAACRB, d1
    and.b   #%11101110, d1
    move.b  d1, CIAACRB
    rts


;-------------------------------------------------------------------------------
; int32_t mclk_get_quantum_timer_elapsed_ns(const MonotonicClock* _Nonnull self)
; Returns the amount of nanoseconds that have elapsed in the current quantum.
_mclk_get_quantum_timer_elapsed_ns:
    cargs cgqte_clock_ptr.l

    move.l  cgqte_clock_ptr(sp), a0

    ; read the current timer value
    moveq.l #0, d1
    move.b  CIAATBHI, d1
    asl.w   #8, d1
    move.b  CIAATBLO, d1

    ; elapsed_ns = (quantum_duration_cycles - current_cycles) * ns_per_cycle
    move.w  mtc_quantum_duration_cycles(a0), d0
    sub.w   d1, d0
    muls    mtc_ns_per_quantum_timer_cycle(a0), d0
    rts


;-------------------------------------------------------------------------------
; Quantums MonotonicClock_GetCurrentQuantums(void)
; Returns the current time in terms of quantums
_MonotonicClock_GetCurrentQuantums:
    move.l  _gMonotonicClockStorage + mtc_current_quantum, d0
    rts
