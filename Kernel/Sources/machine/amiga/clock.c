//
//  clock.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/11/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include <kern/assert.h>
#include <kern/kernlib.h>
#include <kern/timespec.h>
#include <machine/clock.h>
#include <machine/amiga/chipset.h>
#include <machine/irq.h>
#include <sched/sched.h>

extern void mclk_start_ticks(const clock_ref_t _Nonnull self);
extern void mclk_stop_ticks(void);
extern int32_t mclk_get_tick_elapsed_ns(const clock_ref_t _Nonnull self);
void clock_irq(clock_ref_t _Nonnull self, excpt_frame_t* _Nonnull efp);


static struct clock g_mono_clock_storage;
clock_ref_t g_mono_clock = &g_mono_clock_storage;


// Hardware timer usage:
// Amiga: CIA_A_TIMER_B -> monotonic clock ticks


// Initializes the monotonic clock.
void clock_init_mono(clock_ref_t _Nonnull self)
{
    const bool is_ntsc = chipset_is_ntsc();

    // Compute the monotonic clock time resolution:
    //
    // Amiga system clock:
    //  NTSC    28.63636 MHz
    //  PAL     28.37516 MHz
    //
    // CIA A timer B clock:
    //   NTSC    0.715909 MHz (1/10th CPU clock)     [1.3968255 us]
    //   PAL     0.709379 MHz                        [1.4096836 us]
    //
    // Clock tick duration:
    //   NTSC    16.666922 ms    [11932 timer clock cycles]     11932
    //   PAL     16.666689 ms    [11823 timer clock cycles]     11823
    //
    // The clock time resolution is chosen such that:
    // - it is approx 16.667ms (60Hz)
    // - the value is a positive integer in terms of nanoseconds to avoid accumulating / rounding errors as time progresses
    //
    // The ns_per_cia_cycle value is rounded such that:
    // ns_per_cia_cycle * cia_cycles_per_tick <= ns_per_tick

    self->tick_count = 0;
    self->ns_per_tick = (is_ntsc) ? 16666922 : 16666689;
    self->cia_cycles_per_tick = (is_ntsc) ? 11932 : 11823;
    self->ns_per_cia_cycle = (is_ntsc) ? 1396 : 1409;
}

void clock_start(clock_ref_t _Nonnull self)
{
    irq_set_direct_handler(IRQ_ID_MONOTONIC_CLOCK, (irq_direct_func_t)clock_irq, self);
    irq_enable_src(IRQ_ID_CIA_A_TIMER_B);
    mclk_start_ticks(self);
}

void clock_irq(clock_ref_t _Nonnull self, excpt_frame_t* _Nonnull efp)
{
    // update the scheduler clock
    self->tick_count++;


    // run the scheduler
    sched_tick_irq(g_sched, efp);
}

void clock_gettime(clock_ref_t _Nonnull self, struct timespec* _Nonnull ts)
{
    clock_ticks2time(self, self->tick_count, ts);
}

void clock_gettime_hires(clock_ref_t _Nonnull self, struct timespec* _Nonnull ts)
{
    register tick_t ticks;
    register long extra_ns;
    
    do {
        ticks = self->tick_count;
        extra_ns = mclk_get_tick_elapsed_ns(self);
        // Do it again if there was a tick transition while we were busy getting
        // the time values
    } while (self->tick_count != ticks);


    clock_ticks2time(self, ticks, ts);
    ts->tv_nsec += extra_ns;
    if (ts->tv_nsec >= NSEC_PER_SEC) {
        ts->tv_sec++;
        ts->tv_nsec -= NSEC_PER_SEC;
    }
}

tick_t clock_time2ticks(clock_ref_t _Nonnull self, const struct timespec* _Nonnull ts, int rounding)
{
    const int64_t nanos = (int64_t)ts->tv_sec * (int64_t)NSEC_PER_SEC + (int64_t)ts->tv_nsec;
    const int64_t ticks = nanos / (int64_t)self->ns_per_tick;
    
    switch (rounding) {
        case CLOCK_ROUND_TOWARDS_ZERO:
            return ticks;
            
        case CLOCK_ROUND_AWAY_FROM_ZERO: {
            const int64_t nanos_prime = ticks * (int64_t)self->ns_per_tick;
            
            return (nanos_prime < nanos) ? (int32_t)ticks + 1 : (int32_t)ticks;
        }
            
        default:
            abort();
            // NOT REACHED
    }
}

#ifndef MACHINE_AMIGA
void clock_ticks2time(clock_ref_t _Nonnull self, tick_t ticks, struct timespec* _Nonnull ts)
{
    const int64_t ns = (int64_t)ticks * (int64_t)self->ns_per_tick;
    
    ts->tv_sec = ns / (int64_t)NSEC_PER_SEC;
    ts->tv_nsec = ns - ((int64_t)ts->tv_sec * (int64_t)NSEC_PER_SEC);
}
#endif
