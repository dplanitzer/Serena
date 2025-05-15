//
//  MonotonicClock.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/11/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "MonotonicClock.h"
#include "InterruptController.h"
#include <hal/Platform.h>
#include <hal/SystemDescription.h>
#include <kern/kernlib.h>

static void MonotonicClock_OnInterrupt(MonotonicClock* _Nonnull pClock);

MonotonicClock  gMonotonicClockStorage;
MonotonicClock* gMonotonicClock = &gMonotonicClockStorage;


// CIA timer usage:
// CIA B timer A: monotonic clock tick counter


// Initializes the monotonic clock. The monotonic clock uses the quantum timer
// as its time base.
errno_t MonotonicClock_CreateForLocalCPU(const SystemDescription* pSysDesc)
{
    MonotonicClock* pClock = &gMonotonicClockStorage;
    decl_try_err();

    pClock->current_time = TIMESPEC_ZERO;
    pClock->current_quantum = 0;
    pClock->ns_per_quantum = pSysDesc->quantum_duration_ns;

    InterruptHandlerID irqHandler;
    try(InterruptController_AddDirectInterruptHandler(gInterruptController,
                                                      INTERRUPT_ID_QUANTUM_TIMER,
                                                      INTERRUPT_HANDLER_PRIORITY_HIGHEST,
                                                      (InterruptHandler_Closure)MonotonicClock_OnInterrupt,
                                                      pClock,
                                                      &irqHandler));
    InterruptController_SetInterruptHandlerEnabled(gInterruptController, irqHandler, true);

    chipset_start_quantum_timer();
    return EOK;

catch:
    return err;
}

// Returns the current time of the clock in terms of microseconds.
struct timespec MonotonicClock_GetCurrentTime(void)
{
    register const MonotonicClock* pClock = gMonotonicClock;
    register time_t cur_secs;
    register long cur_nanos;
    register long chk_quantum;
    
    do {
        cur_secs = pClock->current_time.tv_sec;
        cur_nanos = pClock->current_time.tv_nsec;
        chk_quantum = pClock->current_quantum;
        
        cur_nanos += chipset_get_quantum_timer_elapsed_ns();
        if (cur_nanos >= ONE_SECOND_IN_NANOS) {
            cur_secs++;
            cur_nanos -= ONE_SECOND_IN_NANOS;
        }
        
        // Do it again if there was a quantum transition while we were busy computing
        // the time
    } while (pClock->current_quantum != chk_quantum);

    return timespec_from(cur_secs, cur_nanos);
}

static void MonotonicClock_OnInterrupt(MonotonicClock* _Nonnull pClock)
{
    // update the scheduler clock
    pClock->current_quantum++;
    
    
    // update the metric time
    pClock->current_time.tv_nsec += pClock->ns_per_quantum;
    if (pClock->current_time.tv_nsec >= ONE_SECOND_IN_NANOS) {
        pClock->current_time.tv_sec++;
        pClock->current_time.tv_nsec -= ONE_SECOND_IN_NANOS;
    }
}

// Blocks the caller until 'deadline'. Returns true if the function did the
// necessary delay and false if the caller should do something else instead to
// achieve the desired delay. Eg context switch to another virtual processor.
// Note that this function is only willing to block the caller for at most a
// millisecond. Longer delays should be done via a scheduler wait().
bool MonotonicClock_DelayUntil(struct timespec deadline)
{
    const struct timespec t_start = MonotonicClock_GetCurrentTime();
    const struct timespec t_delta = timespec_sub(deadline, t_start);
    
    if (t_delta.tv_sec > 0 || (t_delta.tv_sec == 0 && t_delta.tv_nsec > 1000*1000)) {
        return false;
    }
    
    // Just spin for now (would be nice to put the CPU to sleep though for a few micros before rechecking the time or so)
    for (;;) {
        const struct timespec t_cur = MonotonicClock_GetCurrentTime();
        
        if (timespec_gtq(t_cur, deadline)) {
            return true;
        }
    }
    
    // not reached
    return true;
}

// Converts a time interval to a quantum value. The quantum value is rounded
// based on the 'rounding' parameter.
Quantums Quantums_MakeFromTimespec(struct timespec ti, int rounding)
{
    register MonotonicClock* pClock = gMonotonicClock;
    const int64_t nanos = (int64_t)ti.tv_sec * (int64_t)ONE_SECOND_IN_NANOS + (int64_t)ti.tv_nsec;
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
struct timespec Timespec_MakeFromQuantums(Quantums quants)
{
    register MonotonicClock* pClock = gMonotonicClock;
    const int64_t ns = (int64_t)quants * (int64_t)pClock->ns_per_quantum;
    const int32_t secs = ns / (int64_t)ONE_SECOND_IN_NANOS;
    const int32_t nanos = ns - ((int64_t)secs * (int64_t)ONE_SECOND_IN_NANOS);
    
    return timespec_from(secs, nanos);
}
