//
//  MonotonicClock.c
//  Apollo
//
//  Created by Dietmar Planitzer on 2/11/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "MonotonicClock.h"
#include "Platform.h"
#include "SystemDescription.h"
#include "InterruptController.h"

static void MonotonicClock_OnInterrupt(MonotonicClock* _Nonnull pClock);


const TimeInterval kTimeInterval_Zero = {0, 0};
const TimeInterval kTimeInterval_Infinity = {INT32_MAX, ONE_SECOND_IN_NANOS};
const TimeInterval kTimeInterval_MinusInfinity = {INT32_MIN, ONE_SECOND_IN_NANOS};


// CIA timer usage:
// CIA B timer A: monotonic clock tick counter


// Initializes the monotonic clock. The monotonic clock uses the quantum timer
// as its time base.
void MonotonicClock_Init(void)
{
    MonotonicClock* pClock = MonotonicClock_GetShared();
    const SystemDescription* pSysDesc = SystemDescription_GetShared();

    pClock->current_time = TimeInterval_Make(0, 0);
    pClock->current_quantum = 0;
    pClock->ns_per_quantum = pSysDesc->quantum_duration_ns;

    const Int irqHandler = InterruptController_AddDirectInterruptHandler(InterruptController_GetShared(),
                                                  INTERRUPT_ID_QUANTUM_TIMER,
                                                  INTERRUPT_HANDLER_PRIORITY_HIGHEST,
                                                  (InterruptHandler_Closure)MonotonicClock_OnInterrupt,
                                                  (Byte*)pClock);
    InterruptController_SetInterruptHandlerEnabled(InterruptController_GetShared(), irqHandler, true);

    chipset_start_quantum_timer();
}

// Returns the current time of the clock in terms of microseconds.
TimeInterval MonotonicClock_GetCurrentTime(void)
{
    register const MonotonicClock* pClock = MonotonicClock_GetShared();
    register Int32 cur_secs, cur_nanos;
    register Int32 chk_quantum;
    
    do {
        cur_secs = pClock->current_time.seconds;
        cur_nanos = pClock->current_time.nanoseconds;
        chk_quantum = pClock->current_quantum;
        
        cur_nanos += chipset_get_quantum_timer_elapsed_ns();
        if (cur_nanos >= ONE_SECOND_IN_NANOS) {
            cur_secs++;
            cur_nanos -= ONE_SECOND_IN_NANOS;
        }
        
        // Do it again if there was a quantum transition while we were busy computing
        // the time
    } while (pClock->current_quantum != chk_quantum);

    return TimeInterval_Make(cur_secs, cur_nanos);
}

static void MonotonicClock_OnInterrupt(MonotonicClock* _Nonnull pClock)
{
    // update the scheduler clock
    pClock->current_quantum++;
    
    
    // update the metric time
    pClock->current_time.nanoseconds += pClock->ns_per_quantum;
    if (pClock->current_time.nanoseconds >= ONE_SECOND_IN_NANOS) {
        pClock->current_time.seconds++;
        pClock->current_time.nanoseconds -= ONE_SECOND_IN_NANOS;
    }
}

// Blocks the caller until 'deadline'. Returns true if the function did the
// necessary delay and false if the caller should do something else instead to
// achieve the desired delay. Eg context switch to another virtual processor.
// Note that this function is only willing to block the caller for at most a
// millisecond. Longer delays should be done via a scheduler wait().
Bool MonotonicClock_DelayUntil(TimeInterval deadline)
{
    const TimeInterval t_start = MonotonicClock_GetCurrentTime();
    const TimeInterval t_delta = TimeInterval_Subtract(deadline, t_start);
    
    if (t_delta.seconds > 0 || (t_delta.seconds == 0 && t_delta.nanoseconds > 1000*1000)) {
        return false;
    }
    
    // Just spin for now (would be nice to put the CPU to sleep though for a few micros before rechecking the time or so)
    for (;;) {
        const TimeInterval t_cur = MonotonicClock_GetCurrentTime();
        
        if (TimeInterval_GreaterEquals(t_cur, deadline)) {
            return true;
        }
    }
    
    // not reached
    return true;
}


////////////////////////////////////////////////////////////////////////////////


// Converts a time interval to a quantum value. The quantum value is rounded
// based on the 'rounding' parameter.
Quantums Quantums_MakeFromTimeInterval(TimeInterval ti, Int rounding)
{
    MonotonicClock* pClock = MonotonicClock_GetShared();
    const Int64 nanos = (Int64)ti.seconds * (Int64)ONE_SECOND_IN_NANOS + (Int64)ti.nanoseconds;
    const Int64 quants = nanos / (Int64)pClock->ns_per_quantum;
    
    switch (rounding) {
        case QUANTUM_ROUNDING_TOWARDS_ZERO:
            return quants;
            
        case QUANTUM_ROUNDING_AWAY_FROM_ZERO: {
            const Int64 nanos_prime = quants * (Int64)pClock->ns_per_quantum;
            
            return (nanos_prime < nanos) ? (Int32)quants + 1 : (Int32)quants;
        }
            
        default:
            abort();
            return 0;
    }
}

// Converts a quantum value to a time interval.
TimeInterval TimeInterval_MakeFromQuantums(Quantums quants)
{
    MonotonicClock* pClock = MonotonicClock_GetShared();
    const Int64 ns = (Int64)quants * (Int64)pClock->ns_per_quantum;
    const Int32 secs = ns / (Int64)ONE_SECOND_IN_NANOS;
    const Int32 nanos = ns - ((Int64)secs * (Int64)ONE_SECOND_IN_NANOS);
    
    return TimeInterval_Make(secs, nanos);
}

TimeInterval TimeInterval_Add(TimeInterval t0, TimeInterval t1)
{
    TimeInterval ti;
    
    ti.seconds = t0.seconds + t1.seconds;
    ti.nanoseconds = t0.nanoseconds + t1.nanoseconds;
    if (ti.nanoseconds >= ONE_SECOND_IN_NANOS) {
        // handle carry
        ti.seconds++;
        ti.nanoseconds -= ONE_SECOND_IN_NANOS;
    }
    
    // Saturate on overflow
    // See Assembly Language and Systems Programming for the M68000 Family p41
    if ((t0.seconds >= 0 && t1.seconds >= 0 && ti.seconds < 0) || (t0.seconds < 0 && t1.seconds < 0 && ti.seconds >= 0)) {
        ti = (TimeInterval_IsNegative(t0) && TimeInterval_IsNegative(t1)) ? kTimeInterval_MinusInfinity : kTimeInterval_Infinity;
    }
    
    return ti;
}

TimeInterval TimeInterval_Subtract(TimeInterval t0, TimeInterval t1)
{
    TimeInterval ti;
    
    if (TimeInterval_Greater(t0, t1)) {
        // t0 > t1
        ti.seconds = t0.seconds - t1.seconds;
        ti.nanoseconds = t0.nanoseconds - t1.nanoseconds;
        if (ti.nanoseconds < 0) {
            // handle borrow
            ti.nanoseconds += ONE_SECOND_IN_NANOS;
            ti.seconds--;
        }
    } else {
        // t0 <= t1 -> swap t0 and t1 and negate the result
        ti.seconds = t1.seconds - t0.seconds;
        ti.nanoseconds = t1.nanoseconds - t0.nanoseconds;
        if (ti.nanoseconds < 0) {
            // handle borrow
            ti.nanoseconds += ONE_SECOND_IN_NANOS;
            ti.seconds--;
        }
        if (ti.seconds != 0) {
            ti.seconds = -ti.seconds;
        } else {
            ti.nanoseconds = -ti.nanoseconds;
        }
    }

    // Saturate on overflow
    // See Assembly Language and Systems Programming for the M68000 Family p41
    if ((t0.seconds < 0 && t1.seconds >= 0 && ti.seconds >= 0) || (t0.seconds >= 0 && t1.seconds < 0 && ti.seconds < 0)) {
        ti = (TimeInterval_IsNegative(t0) && TimeInterval_IsNegative(t1)) ? kTimeInterval_MinusInfinity : kTimeInterval_Infinity;
    }
    
    return ti;
}
