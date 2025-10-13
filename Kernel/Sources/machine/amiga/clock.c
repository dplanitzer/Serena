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

extern void mclk_start_heartbeat(const clock_ref_t _Nonnull self);
extern void mclk_stop_heartbeat(void);
extern int32_t mclk_get_heartbeat_elapsed_ns(const clock_ref_t _Nonnull self);
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

    self->current_time = TIMESPEC_ZERO;
    self->tick_count = 0;
    self->ns_per_tick = (is_ntsc) ? 16666922 : 16666689;
    self->cia_cycles_per_tick = (is_ntsc) ? 11932 : 11823;
    self->ns_per_cia_cycle = (is_ntsc) ? 1396 : 1409;
}

void clock_start(clock_ref_t _Nonnull self)
{
    irq_set_direct_handler(IRQ_ID_MONOTONIC_CLOCK, (irq_direct_func_t)clock_irq, self);
    irq_enable_src(IRQ_ID_CIA_A_TIMER_B);
    mclk_start_heartbeat(self);
}

void clock_irq(clock_ref_t _Nonnull self, excpt_frame_t* _Nonnull efp)
{
    // update the scheduler clock
    self->tick_count++;
    
    
    // update the metric time
    self->current_time.tv_nsec += self->ns_per_tick;
    if (self->current_time.tv_nsec >= NSEC_PER_SEC) {
        self->current_time.tv_sec++;
        self->current_time.tv_nsec -= NSEC_PER_SEC;
    }


    // run the scheduler
    sched_tick_irq(g_sched, efp);
}

void clock_gettime(clock_ref_t _Nonnull self, struct timespec* _Nonnull ts)
{
    register time_t cur_secs;
    register long cur_nanos;
    register long chk_ticks;
    
    do {
        cur_secs = self->current_time.tv_sec;
        cur_nanos = self->current_time.tv_nsec;
        chk_ticks = self->tick_count;
        
        cur_nanos += mclk_get_heartbeat_elapsed_ns(self);
        if (cur_nanos >= NSEC_PER_SEC) {
            cur_secs++;
            cur_nanos -= NSEC_PER_SEC;
        }
        
        // Do it again if there was a tick transition while we were busy computing
        // the time
    } while (self->tick_count != chk_ticks);

    timespec_from(ts, cur_secs, cur_nanos);
}

void clock_delay(clock_ref_t _Nonnull self, long ns)
{
    struct timespec now, deadline, delta;
    
    clock_gettime(self, &now);
    timespec_from(&delta, 0, ns);
    timespec_add(&now, &delta, &deadline);

    // Just spin for now (would be nice to put the CPU to sleep though for a few micros before rechecking the time or so)
    while (timespec_lt(&now, &deadline)) {
        clock_gettime(self, &now);
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

void clock_ticks2time(clock_ref_t _Nonnull self, tick_t ticks, struct timespec* _Nonnull ts)
{
    const int64_t ns = (int64_t)ticks * (int64_t)self->ns_per_tick;
    
    ts->tv_sec = ns / (int64_t)NSEC_PER_SEC;
    ts->tv_nsec = ns - ((int64_t)ts->tv_sec * (int64_t)NSEC_PER_SEC);
}
