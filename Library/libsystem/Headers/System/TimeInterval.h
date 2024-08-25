//
//  TimeInterval.h
//  libsystem
//
//  Created by Dietmar Planitzer on 2/12/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef _SYS_TIMEINTERVAL_H
#define _SYS_TIMEINTERVAL_H

#include <System/_cmndef.h>
#include <System/abi/_bool.h>
#include <System/abi/_inttypes.h>
#include <System/_time.h>



// Represents time as measured in seconds and nanoseconds. All TimeInterval
// functions expect time interval inputs in canonical form - meaning the
// nanoseconds field is in the range [0..1000_000_000). Negative time interval
// values are represented with a negative seconds field if seconds != 0 and a
// negative nanoseconds field if seconds == 0 and nanoseconds != 0.
//
// TimeInterval is a saturating type. This means that a time value is set to
// +/-infinity on overflow/underflow.
typedef struct timespec TimeInterval;


inline TimeInterval TimeInterval_Make(time_t seconds, long nanoseconds) {
    TimeInterval ti;
    ti.tv_sec = seconds;
    ti.tv_nsec = nanoseconds;
    return ti;
}

inline TimeInterval TimeInterval_MakeSeconds(time_t seconds) {
    TimeInterval ti;
    ti.tv_sec = seconds;
    ti.tv_nsec = 0l;
    return ti;
}

inline TimeInterval TimeInterval_MakeMilliseconds(mseconds_t millis) {
    TimeInterval ti;
    ti.tv_sec = millis / 1000l;
    ti.tv_nsec = (millis - (ti.tv_sec * 1000l)) * 1000l * 1000l;
    return ti;
}

inline TimeInterval TimeInterval_MakeMicroseconds(useconds_t micros) {
    TimeInterval ti;
    ti.tv_sec = micros / (1000l * 1000l);
    ti.tv_nsec = (micros - (ti.tv_sec * 1000l * 1000l)) * 1000l;
    return ti;
}


inline time_t TimeInterval_GetSeconds(TimeInterval ti) {
    return ti.tv_sec;
}

inline mseconds_t TimeInterval_GetMillis(TimeInterval ti) {
    return ti.tv_sec * 1000l + ti.tv_nsec / (1000l * 1000l);
}

inline useconds_t TimeInterval_GetMicros(TimeInterval ti) {
    return ti.tv_sec * 1000l * 1000l + ti.tv_nsec / 1000l;
}

inline int64_t TimeInterval_GetNanos(TimeInterval ti) {
    return (int64_t)ti.tv_sec * 1000ll * 1000ll * 1000ll + (int64_t)ti.tv_nsec;
}


inline bool TimeInterval_IsNegative(TimeInterval ti) {
    return ti.tv_sec < 0 || ti.tv_nsec < 0;
}


inline bool TimeInterval_Equals(TimeInterval t0, TimeInterval t1) {
    return t0.tv_nsec == t1.tv_nsec && t0.tv_sec == t1.tv_sec;
}

inline bool TimeInterval_Less(TimeInterval t0, TimeInterval t1) {
    return (t0.tv_sec < t1.tv_sec || (t0.tv_sec == t1.tv_sec && t0.tv_nsec < t1.tv_nsec));
}

inline bool TimeInterval_LessEquals(TimeInterval t0, TimeInterval t1) {
    return (t0.tv_sec < t1.tv_sec || (t0.tv_sec == t1.tv_sec && t0.tv_nsec <= t1.tv_nsec));
}

inline bool TimeInterval_Greater(TimeInterval t0, TimeInterval t1) {
    return (t0.tv_sec > t1.tv_sec || (t0.tv_sec == t1.tv_sec && t0.tv_nsec > t1.tv_nsec));
}

inline bool TimeInterval_GreaterEquals(TimeInterval t0, TimeInterval t1) {
    return (t0.tv_sec > t1.tv_sec || (t0.tv_sec == t1.tv_sec && t0.tv_nsec >= t1.tv_nsec));
}


extern TimeInterval TimeInterval_Add(TimeInterval t0, TimeInterval t1);

extern TimeInterval TimeInterval_Subtract(TimeInterval t0, TimeInterval t1);


extern const TimeInterval   kTimeInterval_Zero;
extern const TimeInterval   kTimeInterval_Infinity;
extern const TimeInterval   kTimeInterval_MinusInfinity;

#endif /* _SYS_TIMEINTERVAL_H */
