//
//  TimeInterval.h
//  Apollo
//
//  Created by Dietmar Planitzer on 2/2/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef TimeInterval_h
#define TimeInterval_h

#include <klib/Types.h>


// Represents time as measured in seconds and nanoseconds. All TimeInterval functions
// expect time interval inputs in canonical form - meaning the nanoseconds field
// is in the range [0..1000_000_000). Negative time interval values are represented
// with a negative seconds field if seconds != 0 and a negative nanoseconds field if
// seconds == 0 and nanoseconds != 0.
// The TimeInterval type is a saturating type. This means that a time value is set to
// +/-infinity if a computation overflows / underflows.
typedef struct _TimeInterval {
    Int32   seconds;
    Int32   nanoseconds;        // 0..<1billion
} TimeInterval;


inline TimeInterval TimeInterval_Make(Int32 seconds, Int32 nanoseconds) {
    TimeInterval ti;
    ti.seconds = seconds;
    ti.nanoseconds = nanoseconds;
    return ti;
}

inline TimeInterval TimeInterval_MakeSeconds(Int32 seconds) {
    TimeInterval ti;
    ti.seconds = seconds;
    ti.nanoseconds = 0;
    return ti;
}

inline TimeInterval TimeInterval_MakeMilliseconds(Int32 millis) {
    TimeInterval ti;
    ti.seconds = millis / 1000;
    ti.nanoseconds = (millis - (ti.seconds * 1000)) * 1000 * 1000;
    return ti;
}

inline TimeInterval TimeInterval_MakeMicroseconds(Int32 micros) {
    TimeInterval ti;
    ti.seconds = micros / (1000 * 1000);
    ti.nanoseconds = (micros - (ti.seconds * 1000 * 1000)) * 1000;
    return ti;
}

inline Bool TimeInterval_IsNegative(TimeInterval ti) {
    return ti.seconds < 0 || ti.nanoseconds < 0;
}

inline Bool TimeInterval_Equals(TimeInterval t0, TimeInterval t1) {
    return t0.nanoseconds == t1.nanoseconds && t0.seconds == t1.seconds;
}

inline Bool TimeInterval_Less(TimeInterval t0, TimeInterval t1) {
    return (t0.seconds < t1.seconds || (t0.seconds == t1.seconds && t0.nanoseconds < t1.nanoseconds));
}

inline Bool TimeInterval_LessEquals(TimeInterval t0, TimeInterval t1) {
    return (t0.seconds < t1.seconds || (t0.seconds == t1.seconds && t0.nanoseconds <= t1.nanoseconds));
}

inline Bool TimeInterval_Greater(TimeInterval t0, TimeInterval t1) {
    return (t0.seconds > t1.seconds || (t0.seconds == t1.seconds && t0.nanoseconds > t1.nanoseconds));
}

inline Bool TimeInterval_GreaterEquals(TimeInterval t0, TimeInterval t1) {
    return (t0.seconds > t1.seconds || (t0.seconds == t1.seconds && t0.nanoseconds >= t1.nanoseconds));
}

extern TimeInterval TimeInterval_Add(TimeInterval t0, TimeInterval t1);

extern TimeInterval TimeInterval_Subtract(TimeInterval t0, TimeInterval t1);


#define ONE_SECOND_IN_NANOS (1000 * 1000 * 1000)
#define kQuantums_Infinity      INT32_MAX
#define kQuantums_MinusInfinity INT32_MIN
extern const TimeInterval       kTimeInterval_Zero;
extern const TimeInterval       kTimeInterval_Infinity;
extern const TimeInterval       kTimeInterval_MinusInfinity;

#endif /* TimeInterval_h */
