//
//  clock.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/11/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include <ext/timespec.h>
#include <hal/clock.h>
#include <hal/hw/m68k-amiga/chipset.h>
#include <hal/irq.h>
#include <kern/assert.h>
#include <kern/kernlib.h>
#include <sched/sched.h>

struct ticks_ns {
    tick_t  ticks;
    long    ns;
};

extern void _clock_start_ticker(const clock_ref_t _Nonnull self);
extern void _clock_stop_ticker(void);
extern void _clock_getticks_ns(const clock_ref_t _Nonnull self, struct ticks_ns* _Nonnull tnp);
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
    self->deadline_queue = NULL;
    self->ns_per_tick = (is_ntsc) ? 16666922 : 16666689;
    self->cia_cycles_per_tick = (is_ntsc) ? 11932 : 11823;
    self->ns_per_cia_cycle = (is_ntsc) ? 1396 : 1409;
}

void clock_start(clock_ref_t _Nonnull self)
{
    irq_set_direct_handler(IRQ_ID_MONOTONIC_CLOCK, (irq_direct_func_t)clock_irq, self);
    irq_enable_src(IRQ_ID_CIA_A_TIMER_B);
    _clock_start_ticker(self);
}

void clock_irq(clock_ref_t _Nonnull self, excpt_frame_t* _Nonnull efp)
{
    // update the scheduler clock
    self->tick_count++;


    // execute one-shot timers
    register const tick_t now = self->tick_count;
    for (;;) {
        register clock_deadline_t* cp = self->deadline_queue;
        
        if (cp == NULL || cp->deadline > now) {
            break;
        }
        
        self->deadline_queue = cp->next;
        cp->next = NULL;
        cp->isArmed = false;

        cp->func(cp->arg);
    }


    // run the scheduler
    sched_tick_irq(g_sched, efp);
}

void clock_deadline(clock_ref_t _Nonnull self, clock_deadline_t* _Nonnull deadline)
{
    const unsigned sim = irq_set_mask(IRQ_MASK_CIA_A);
    register clock_deadline_t* pp = NULL;
    register clock_deadline_t* cp = self->deadline_queue;

    assert(!deadline->isArmed);

    while (cp && cp->deadline <= deadline->deadline) {
        pp = cp;
        cp = cp->next;
    }
    
    if (pp) {
        deadline->next = pp->next;
        pp->next = deadline;
    }
    else {
        deadline->next = self->deadline_queue;
        self->deadline_queue = deadline;
    }
    deadline->isArmed = true;

    irq_restore_mask(sim);
}

bool clock_cancel_deadline(clock_ref_t _Nonnull self, clock_deadline_t* _Nonnull deadline)
{
    const unsigned sim = irq_set_mask(IRQ_MASK_CIA_A);
    bool r = false;

    if (deadline->isArmed) {
        register clock_deadline_t* pp = NULL;
        register clock_deadline_t* cp = self->deadline_queue;

        while (cp) {
            if (cp == deadline) {
                if (pp) {
                    pp->next = cp->next;
                }
                else {
                    self->deadline_queue = cp->next;
                }
                break;
            }

            pp = cp;
            cp = cp->next;
        }

        deadline->next = NULL;
        deadline->isArmed = false;
        r = true;
    }

    irq_restore_mask(sim);

    return r;
}

void clock_gettime(clock_ref_t _Nonnull self, struct timespec* _Nonnull ts)
{
    clock_ticks2time(self, self->tick_count, ts);
}

void clock_gettime_hires(clock_ref_t _Nonnull self, struct timespec* _Nonnull ts)
{
    struct ticks_ns ticks_ns;
    
    _clock_getticks_ns(self, &ticks_ns);
    clock_ticks2time(self, ticks_ns.ticks, ts);
    ts->tv_nsec += ticks_ns.ns;
    if (ts->tv_nsec >= NSEC_PER_SEC) {
        ts->tv_sec++;
        ts->tv_nsec -= NSEC_PER_SEC;
    }
}

#ifndef MACHINE_AMIGA
tick_t clock_time2ticks_floor(clock_ref_t _Nonnull self, const struct timespec* _Nonnull ts)
{
    const int64_t nanos = (int64_t)ts->tv_sec * (int64_t)NSEC_PER_SEC + (int64_t)ts->tv_nsec;
    
    return nanos / (int64_t)self->ns_per_tick;
}

tick_t clock_time2ticks_ceil(clock_ref_t _Nonnull self, const struct timespec* _Nonnull ts)
{
    const int64_t nanos = (int64_t)ts->tv_sec * (int64_t)NSEC_PER_SEC + (int64_t)ts->tv_nsec;
    const int64_t ticks = nanos / (int64_t)self->ns_per_tick;
    const int64_t nanos_prime = ticks * (int64_t)self->ns_per_tick;
            
    return (nanos_prime < nanos) ? (int32_t)ticks + 1 : (int32_t)ticks;
}

void clock_ticks2time(clock_ref_t _Nonnull self, tick_t ticks, struct timespec* _Nonnull ts)
{
    const int64_t ns = (int64_t)ticks * (int64_t)self->ns_per_tick;
    
    ts->tv_sec = ns / (int64_t)NSEC_PER_SEC;
    ts->tv_nsec = ns - ((int64_t)ts->tv_sec * (int64_t)NSEC_PER_SEC);
}
#endif
