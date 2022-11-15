//
//  MonotonicClock.h
//  Apollo
//
//  Created by Dietmar Planitzer on 2/11/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef MonotonicClock_h
#define MonotonicClock_h

#include "Foundation.h"


typedef Int32 Quantums;             // Time unit of the scheduler clock which increments monotonically and once per quantum interrupt

// Represents metric time based on seconds and nanoseconds in a second. All TimeInterval
// functions expect time interval inputs in canonical form meaning that the nanoseconds
// field is in the range [0..1000_000_000). Negative time interval values are represented
// with a negative seconds field if seconds != 0 and a negative nanoseconds field if
// seconds == 0 and nanoseconds != 0.
// The TimeInterval type is a saturating type. This means that a time value is set to
// +/-infinity if a computation overflows / underflows.
typedef struct _TimeInterval {
    Int32   seconds;
    Int32   nanoseconds;        // 0..<1billion
} TimeInterval;


#define ONE_SECOND_IN_NANOS (1000 * 1000 * 1000)

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


#define kQuantums_Infinity      INT32_MAX
#define kQuantums_MinusInfinity INT32_MIN
extern const TimeInterval       kTimeInterval_Zero;
extern const TimeInterval       kTimeInterval_Infinity;
extern const TimeInterval       kTimeInterval_MinusInfinity;


// The monotonic clock
// Note: Keep in sync with lowmem.i
typedef struct _MonotonicClock {
    volatile TimeInterval   current_time;
    volatile Quantums       current_quantum;    // Current scheduler time in terms of elapsed quantums since boot
    Int32                   ns_per_quantum;     // duration of a quantum in terms of nanoseconds
} MonotonicClock;


extern MonotonicClock* _Nonnull MonotonicClock_GetShared(void);
extern void MonotonicCock_Init(void);

extern Quantums MonotonicClock_GetCurrentQuantums(void);
extern TimeInterval MonotonicClock_GetCurrentTime(void);

// Blocks the caller until 'deadline'. Returns true if the function did the
// necessary delay and false if the caller should do something else instead to
// achieve the desired delay. Eg context switch to another virtual processor.
// Note that this function is only willing to block the caller for at most a few
// milliseconds. Longer delays should be done via a scheduler wait().
extern Bool MonotonicClock_DelayUntil(TimeInterval deadline);


// Rounding modes for TimeInterval to Quantums conversion
#define QUANTUM_ROUNDING_TOWARDS_ZERO   0
#define QUANTUM_ROUNDING_AWAY_FROM_ZERO 1

// Converts a time interval to a quantum value. The quantum value is rounded
// based on the 'rounding' parameter.
extern Quantums Quantums_MakeFromTimeInterval(TimeInterval ti, Int rounding);

// Converts a quantum value to a time interval.
extern TimeInterval TimeInterval_MakeFromQuantums(Quantums quants);

#endif /* MonotonicClock_h */
