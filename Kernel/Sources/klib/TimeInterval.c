//
//  TimeInterval.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/9/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include <kern/types.h>
#include <kern/kernlib.h>

const TimeInterval kTimeInterval_Zero = {0l, 0l};
const TimeInterval kTimeInterval_Infinity = {LONG_MAX, ONE_SECOND_IN_NANOS};
const TimeInterval kTimeInterval_MinusInfinity = {LONG_MIN, ONE_SECOND_IN_NANOS};

TimeInterval TimeInterval_Add(TimeInterval t0, TimeInterval t1)
{
    TimeInterval ti;
    
    ti.tv_sec = t0.tv_sec + t1.tv_sec;
    ti.tv_nsec = t0.tv_nsec + t1.tv_nsec;
    if (ti.tv_nsec >= ONE_SECOND_IN_NANOS) {
        // handle carry
        ti.tv_sec++;
        ti.tv_nsec -= ONE_SECOND_IN_NANOS;
    }
    
    // Saturate on overflow
    // See Assembly Language and Systems Programming for the M68000 Family p41
    if ((t0.tv_sec >= 0 && t1.tv_sec >= 0 && ti.tv_sec < 0) || (t0.tv_sec < 0 && t1.tv_sec < 0 && ti.tv_sec >= 0)) {
        ti = (TimeInterval_IsNegative(t0) && TimeInterval_IsNegative(t1)) ? kTimeInterval_MinusInfinity : kTimeInterval_Infinity;
    }
    
    return ti;
}

TimeInterval TimeInterval_Subtract(TimeInterval t0, TimeInterval t1)
{
    TimeInterval ti;
    
    if (TimeInterval_Greater(t0, t1)) {
        // t0 > t1
        ti.tv_sec = t0.tv_sec - t1.tv_sec;
        ti.tv_nsec = t0.tv_nsec - t1.tv_nsec;
        if (ti.tv_nsec < 0) {
            // handle borrow
            ti.tv_nsec += ONE_SECOND_IN_NANOS;
            ti.tv_sec--;
        }
    } else {
        // t0 <= t1 -> swap t0 and t1 and negate the result
        ti.tv_sec = t1.tv_sec - t0.tv_sec;
        ti.tv_nsec = t1.tv_nsec - t0.tv_nsec;
        if (ti.tv_nsec < 0) {
            // handle borrow
            ti.tv_nsec += ONE_SECOND_IN_NANOS;
            ti.tv_sec--;
        }
        if (ti.tv_sec != 0) {
            ti.tv_sec = -ti.tv_sec;
        } else {
            ti.tv_nsec = -ti.tv_nsec;
        }
    }

    // Saturate on overflow
    // See Assembly Language and Systems Programming for the M68000 Family p41
    if ((t0.tv_sec < 0 && t1.tv_sec >= 0 && ti.tv_sec >= 0) || (t0.tv_sec >= 0 && t1.tv_sec < 0 && ti.tv_sec < 0)) {
        ti = (TimeInterval_IsNegative(t0) && TimeInterval_IsNegative(t1)) ? kTimeInterval_MinusInfinity : kTimeInterval_Infinity;
    }
    
    return ti;
}
