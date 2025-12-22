;
;  machine/hw/m68k-amiga/clock_asm.s
;  kernel
;
;  Created by Dietmar Planitzer on 2/2/21.
;  Copyright © 2021 Dietmar Planitzer. All rights reserved.
;

    include "chipset.i"
    include <machine/hw/m68k/lowmem.i>


    xdef __clock_start_ticker
    xdef __clock_stop_ticker
    xdef __clock_getticks_ns
    xdef _clock_time2ticks_floor
    xdef _clock_time2ticks_ceil
    xdef _clock_ticks2time


;-------------------------------------------------------------------------------
; void _clock_start_ticker(const clock_ref_t _Nonnull self)
; Starts the clock running. A hardware timer is used to provide the clock
; heartbeat which is used to increment the clock tick counter.
__clock_start_ticker:
    cargs csqt_clock_ptr.l

    move.l  csqt_clock_ptr(sp), a0

    ; stop the timer
    jsr     __clock_stop_ticker

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
; void _clock_stop_ticker(void)
; Stops the clock.
__clock_stop_ticker:
    move.b  CIAACRB, d1
    and.b   #%11101110, d1
    move.b  d1, CIAACRB
    rts


;-------------------------------------------------------------------------------
; void _clock_getticks_ns(const clock_ref_t _Nonnull self, struct ticks_ns* _Nonnull tnp)
; Returns the current tick and the amount of nanoseconds that have elapsed so
; far in the current clock tick.
__clock_getticks_ns:
    cargs cgtn_clock_ptr.l, cgtn_tn_ptr.l

    move.l  cgtn_clock_ptr(sp), a0
    move.l  cgtn_tn_ptr(sp), a1
    move.l  d2, -(sp)

.1:
    move.l  mtc_tick_count(a0), d0

    ; read the current timer value
    moveq.l #0, d1
    move.b  CIAATBHI, d1
    lsl.w   #8, d1
    move.b  CIAATBLO, d1

    cmp.l   mtc_tick_count(a0), d0
    bne.s   .1

    ; elapsed_ns = (cia_cycles_per_tick - current_cycles) * ns_per_cycle
    moveq.l #0, d2
    move.w  mtc_cia_cycles_per_tick(a0), d2
    sub.w   d1, d2
    mulu.w  mtc_ns_per_cia_cycle(a0), d2

    move.l  d0, 0(a1)
    move.l  d2, 4(a1)

    move.l  (sp)+, d2
    rts


;-------------------------------------------------------------------------------
; tick_t clock_time2ticks_floor(clock_ref_t _Nonnull self, const struct timespec* _Nonnull ts)
;
;    const int64_t nanos = (int64_t)ts->tv_sec * (int64_t)NSEC_PER_SEC + (int64_t)ts->tv_nsec;
;    const tick_t r = nanos / (int64_t)self->ns_per_tick;
;
_clock_time2ticks_floor:
    cargs ct2tf_saved_d2.l, ct2tf_clock_ptr.l, ct2tf_ts_ptr.l

    move.l  d2, -(sp)

    move.l  ct2tf_ts_ptr(sp), a0
    move.l  #$3B9ACA00, d0              ; NSEC_PER_SEC
    mulu.l  0(a0), d1:d0                ; ts->tv_sec * NSEC_PER_SEC -> [d0:d1]

    moveq.l #0, d2
    add.l   4(a0), d0                   ; 64bit add of ts->tv_nsec
    addx.l  d2, d1

    move.l  ct2tf_clock_ptr(sp), a0
    divu.l  mtc_ns_per_tick(a0), d1:d0  ; d0 -> ticks

    move.l  (sp)+, d2
    rts


;-------------------------------------------------------------------------------
; tick_t clock_time2ticks_ceil(clock_ref_t _Nonnull self, const struct timespec* _Nonnull ts)
;
;   const int64_t nanos = (int64_t)ts->tv_sec * (int64_t)NSEC_PER_SEC + (int64_t)ts->tv_nsec;
;   const int64_t ticks = nanos / (int64_t)self->ns_per_tick;
;   const int64_t nanos_prime = ticks * (int64_t)self->ns_per_tick;
;            
;   return (nanos_prime < nanos) ? (int32_t)ticks + 1 : (int32_t)ticks;
;
_clock_time2ticks_ceil:
    cargs ct2tc_saved_d2.l, ct2tc_clock_ptr.l, ct2tc_ts_ptr.l

    move.l  d2, -(sp)

    move.l  ct2tc_ts_ptr(sp), a0
    move.l  #$3B9ACA00, d0              ; NSEC_PER_SEC
    mulu.l  0(a0), d1:d0                ; ts->tv_sec * NSEC_PER_SEC -> [d0:d1]

    moveq.l #0, d2
    add.l   4(a0), d0                   ; 64bit add of ts->tv_nsec
    addx.l  d2, d1

    move.l  ct2tc_clock_ptr(sp), a0
    divu.l  mtc_ns_per_tick(a0), d1:d0  ; [d1:d0] -> remainder, ticks
        
    tst.l   d1
    beq.s   .1
    addq.l  #1, d0
    bne.s   .1
    subq.l  #1, d0                      ; the +1 overflowed -> get us back to kTick_Infinity

.1:
    move.l (sp)+, d2
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
    mulu.l  ct2t_ticks(sp), d0:d1       ; [d0:d1] -> ns
    
    move.l  ct2t_ts_ptr(sp), a0
    divu.l  #$3B9ACA00, d0:d1           ; sec = ns / NSEC_PER_SEC
    move.l  d1, 0(a0)                   ; d1 (quotient)  -> tv_sec
    move.l  d0, 4(a0)                   ; d0 (remainder) -> tv_nsec

    rts
