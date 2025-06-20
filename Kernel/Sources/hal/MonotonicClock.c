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
#include <kern/timespec.h>


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
        
        cur_nanos += chipset_get_quantum_timer_elapsed_ns();
        if (cur_nanos >= ONE_SECOND_IN_NANOS) {
            cur_secs++;
            cur_nanos -= ONE_SECOND_IN_NANOS;
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
    if (pClock->current_time.tv_nsec >= ONE_SECOND_IN_NANOS) {
        pClock->current_time.tv_sec++;
        pClock->current_time.tv_nsec -= ONE_SECOND_IN_NANOS;
    }
}

bool MonotonicClock_Delay(const struct timespec* _Nonnull timeout)
{
    struct timespec now, deadline;
    
    MonotonicClock_GetCurrentTime(&now);
    timespec_add(&now, timeout, &deadline);
    
    if (timeout->tv_sec > 0l || (timeout->tv_sec == 0 && timeout->tv_nsec > 1000l*1000l)) {
        return false;
    }
    
    // Just spin for now (would be nice to put the CPU to sleep though for a few micros before rechecking the time or so)
    for (;;) {
        MonotonicClock_GetCurrentTime(&now); 

        if (timespec_ge(&now, &deadline)) {
            return true;
        }
    }
    
    // not reached
    return true;
}

// Converts a time interval to a quantum value. The quantum value is rounded
// based on the 'rounding' parameter.
Quantums Quantums_MakeFromTimespec(const struct timespec* _Nonnull ts, int rounding)
{
    register MonotonicClock* pClock = gMonotonicClock;
    const int64_t nanos = (int64_t)ts->tv_sec * (int64_t)ONE_SECOND_IN_NANOS + (int64_t)ts->tv_nsec;
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
    
    ts->tv_sec = ns / (int64_t)ONE_SECOND_IN_NANOS;
    ts->tv_nsec = ns - ((int64_t)ts->tv_sec * (int64_t)ONE_SECOND_IN_NANOS);
}
