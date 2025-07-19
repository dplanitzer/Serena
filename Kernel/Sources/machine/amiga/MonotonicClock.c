//
//  MonotonicClock.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/11/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include <kern/kernlib.h>
#include <kern/timespec.h>
#include <machine/MonotonicClock.h>
#include <machine/InterruptController.h>
#include <machine/SystemDescription.h>
#include <machine/amiga/chipset.h>

extern void mclk_start_quantum_timer(const MonotonicClock* _Nonnull self);
extern void mclk_stop_quantum_timer(void);
extern int32_t mclk_get_quantum_timer_elapsed_ns(const MonotonicClock* _Nonnull self);


static void MonotonicClock_OnInterrupt(MonotonicClock* _Nonnull pClock);

MonotonicClock  gMonotonicClockStorage;
MonotonicClock* gMonotonicClock = &gMonotonicClockStorage;


// CIA timer usage:
// CIA B timer A: monotonic clock tick counter


// Initializes the monotonic clock. The monotonic clock uses the quantum timer
// as its time base.
errno_t MonotonicClock_CreateForLocalCPU(const SystemDescription* pSysDesc)
{
    decl_try_err();
    MonotonicClock* pClock = &gMonotonicClockStorage;
    const bool is_ntsc = chipset_is_ntsc();

    // Compute the quantum timer parameters:
    //
    // Amiga system clock:
    //  NTSC    28.63636 MHz
    //  PAL     28.37516 MHz
    //
    // CIA B timer A clock:
    //   NTSC    0.715909 MHz (1/10th CPU clock)     [1.3968255 us]
    //   PAL     0.709379 MHz                        [1.4096836 us]
    //
    // Quantum duration:
    //   NTSC    16.761906 ms    [12000 timer clock cycles]
    //   PAL     17.621045 ms    [12500 timer clock cycles]
    //
    // The quantum duration is chosen such that:
    // - it is approx 16ms - 17ms
    // - the value is a positive integer in terms of nanoseconds to avoid accumulating / rounding errors as time progresses
    //
    // The ns_per_quantum_timer_cycle value is rounded such that:
    // ns_per_quantum_timer_cycle * quantum_duration_cycles <= quantum_duration_ns

    pClock->current_time = TIMESPEC_ZERO;
    pClock->current_quantum = 0;
    pClock->ns_per_quantum = (is_ntsc) ? 16761906 : 17621045;
    pClock->quantum_duration_cycles = (is_ntsc) ? 12000 : 12500;
    pClock->ns_per_quantum_timer_cycle = (is_ntsc) ? 1396 : 1409;

    InterruptHandlerID irqHandler;
    try(InterruptController_AddDirectInterruptHandler(gInterruptController,
                                                      INTERRUPT_ID_QUANTUM_TIMER,
                                                      INTERRUPT_HANDLER_PRIORITY_HIGHEST,
                                                      (InterruptHandler_Closure)MonotonicClock_OnInterrupt,
                                                      pClock,
                                                      &irqHandler));
    InterruptController_SetInterruptHandlerEnabled(gInterruptController, irqHandler, true);

    mclk_start_quantum_timer(pClock);
    return EOK;

catch:
    return err;
}

void MonotonicClock_GetCurrentTime(struct timespec* _Nonnull ts)
{
    register const MonotonicClock* pClock = gMonotonicClock;
    register time_t cur_secs;
    register long cur_nanos;
    register long chk_quantum;
    
    do {
        cur_secs = pClock->current_time.tv_sec;
        cur_nanos = pClock->current_time.tv_nsec;
        chk_quantum = pClock->current_quantum;
        
        cur_nanos += mclk_get_quantum_timer_elapsed_ns(pClock);
        if (cur_nanos >= NSEC_PER_SEC) {
            cur_secs++;
            cur_nanos -= NSEC_PER_SEC;
        }
        
        // Do it again if there was a quantum transition while we were busy computing
        // the time
    } while (pClock->current_quantum != chk_quantum);

    timespec_from(ts, cur_secs, cur_nanos);
}

static void MonotonicClock_OnInterrupt(MonotonicClock* _Nonnull pClock)
{
    // update the scheduler clock
    pClock->current_quantum++;
    
    
    // update the metric time
    pClock->current_time.tv_nsec += pClock->ns_per_quantum;
    if (pClock->current_time.tv_nsec >= NSEC_PER_SEC) {
        pClock->current_time.tv_sec++;
        pClock->current_time.tv_nsec -= NSEC_PER_SEC;
    }
}

void MonotonicClock_Delay(long ns)
{
    struct timespec now, deadline, delta;
    
    MonotonicClock_GetCurrentTime(&now);
    timespec_from(&delta, 0, ns);
    timespec_add(&now, &delta, &deadline);

    // Just spin for now (would be nice to put the CPU to sleep though for a few micros before rechecking the time or so)
    while (timespec_lt(&now, &deadline)) {
        MonotonicClock_GetCurrentTime(&now);
    }
}

// Converts a time interval to a quantum value. The quantum value is rounded
// based on the 'rounding' parameter.
Quantums Quantums_MakeFromTimespec(const struct timespec* _Nonnull ts, int rounding)
{
    register MonotonicClock* pClock = gMonotonicClock;
    const int64_t nanos = (int64_t)ts->tv_sec * (int64_t)NSEC_PER_SEC + (int64_t)ts->tv_nsec;
    const int64_t quants = nanos / (int64_t)pClock->ns_per_quantum;
    
    switch (rounding) {
        case QUANTUM_ROUNDING_TOWARDS_ZERO:
            return quants;
            
        case QUANTUM_ROUNDING_AWAY_FROM_ZERO: {
            const int64_t nanos_prime = quants * (int64_t)pClock->ns_per_quantum;
            
            return (nanos_prime < nanos) ? (int32_t)quants + 1 : (int32_t)quants;
        }
            
        default:
            abort();
            return 0;
    }
}

// Converts a quantum value to a time interval.
void Timespec_MakeFromQuantums(struct timespec* _Nonnull ts, Quantums quants)
{
    register MonotonicClock* pClock = gMonotonicClock;
    const int64_t ns = (int64_t)quants * (int64_t)pClock->ns_per_quantum;
    
    ts->tv_sec = ns / (int64_t)NSEC_PER_SEC;
    ts->tv_nsec = ns - ((int64_t)ts->tv_sec * (int64_t)NSEC_PER_SEC);
}
