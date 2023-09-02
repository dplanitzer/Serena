//
//  TimeInterval.c
//  Apollo
//
//  Created by Dietmar Planitzer on 2/9/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "TimeInterval.h"


#define ONE_SECOND_IN_NANOS (1000 * 1000 * 1000)
const TimeInterval kTimeInterval_Zero = {0, 0};
const TimeInterval kTimeInterval_Infinity = {INT32_MAX, ONE_SECOND_IN_NANOS};
const TimeInterval kTimeInterval_MinusInfinity = {INT32_MIN, ONE_SECOND_IN_NANOS};

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
