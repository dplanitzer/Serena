;
;  machine/amiga/clock.s
;  kernel
;
;  Created by Dietmar Planitzer on 2/2/21.
;  Copyright © 2021 Dietmar Planitzer. All rights reserved.
;

    include "chipset.i"
    include <machine/lowmem.i>


    xdef _mclk_start_ticks
    xdef _mclk_stop_ticks
    xdef _mclk_get_tick_elapsed_ns
    xdef _clock_ticks2time


;-------------------------------------------------------------------------------
; void mclk_start_ticks(const clock_ref_t _Nonnull self)
; Starts the clock running. A hardware timer is used to provide the clock
; heartbeat which is used to increment the clock tick counter.
_mclk_start_ticks:
    cargs csqt_clock_ptr.l

    move.l  csqt_clock_ptr(sp), a0

    ; stop the timer
    jsr     _mclk_stop_ticks

    ; load the timer with the new ticks value
    move.w  mtc_cia_cycles_per_tick(a0), d0
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
; void mclk_stop_ticks(void)
; Stops the clock.
_mclk_stop_ticks:
    move.b  CIAACRB, d1
    and.b   #%11101110, d1
    move.b  d1, CIAACRB
    rts


;-------------------------------------------------------------------------------
; int32_t mclk_get_tick_elapsed_ns(const clock_ref_t _Nonnull self)
; Returns the amount of nanoseconds that have elapsed in the current clock tick.
_mclk_get_tick_elapsed_ns:
    cargs cgqte_clock_ptr.l

    move.l  cgqte_clock_ptr(sp), a0

    ; read the current timer value
    moveq.l #0, d1
    move.b  CIAATBHI, d1
    asl.w   #8, d1
    move.b  CIAATBLO, d1

    ; elapsed_ns = (cia_cycles_per_tick - current_cycles) * ns_per_cycle
    move.w  mtc_cia_cycles_per_tick(a0), d0
    sub.w   d1, d0
    muls    mtc_ns_per_cia_cycle(a0), d0
    rts


;-------------------------------------------------------------------------------
; void clock_ticks2time(clock_ref_t _Nonnull self, tick_t ticks, struct timespec* _Nonnull ts)
;
;    const int64_t ns = (int64_t)ticks * (int64_t)self->ns_per_tick;
;    ts->tv_sec = ns / (int64_t)NSEC_PER_SEC;
;    ts->tv_nsec = ns - ((int64_t)ts->tv_sec * (int64_t)NSEC_PER_SEC);
;
_clock_ticks2time:
    cargs ct2t_clock_ptr.l, ct2t_ticks.l, ct2t_ts_ptr.l

    move.l  ct2t_clock_ptr(sp), a0
    move.l  mtc_ns_per_tick(a0), d1
    muls.l  ct2t_ticks(sp), d0:d1       ; [d0, d1] -> ns
    
    move.l  ct2t_ts_ptr(sp), a0
    divs.l  #$3B9ACA00, d0:d1           ; sec = ns / NSEC_PER_SEC
    move.l  d1, 0(a0)                   ; d1 (quotient)  -> tv_sec
    move.l  d0, 4(a0)                   ; d0 (remainder) -> tv_nsec

    rts
