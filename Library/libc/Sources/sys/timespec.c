//
//  timespec.c
//  libc
//
//  Created by Dietmar Planitzer on 2/12/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include <sys/timespec.h>
#include <limits.h>


const struct timespec   TIMESPEC_ZERO = {0l, 0l};
const struct timespec   TIMESPEC_INF = {LONG_MAX, NSEC_PER_SEC-1l};


void timespec_from_ms(struct timespec* _Nonnull ts, mseconds_t millis)
{
    ts->tv_sec = millis / 1000l;
    ts->tv_nsec = (millis - (ts->tv_sec * 1000l)) * 1000l * 1000l;
}

void timespec_from_us(struct timespec* _Nonnull ts, useconds_t micros)
{
    ts->tv_sec = micros / (1000l * 1000l);
    ts->tv_nsec = (micros - (ts->tv_sec * 1000l * 1000l)) * 1000l;
}


mseconds_t timespec_ms(struct timespec* _Nonnull ts)
{
    return ts->tv_sec * 1000l + ts->tv_nsec / (1000l * 1000l);
}

useconds_t timespec_us(struct timespec* _Nonnull ts)
{
    return ts->tv_sec * 1000l * 1000l + ts->tv_nsec / 1000l;
}

int64_t timespec_ns(struct timespec* _Nonnull ts)
{
    return (int64_t)ts->tv_sec * 1000ll * 1000ll * 1000ll + (int64_t)ts->tv_nsec;
}


bool timespec_eq(const struct timespec* _Nonnull t0, const struct timespec* _Nonnull t1)
{
    return (t0->tv_nsec == t1->tv_nsec && t0->tv_sec == t1->tv_sec) ? true : false;
}

bool timespec_lt(const struct timespec* _Nonnull t0, const struct timespec* _Nonnull t1)
{
    return (t0->tv_sec < t1->tv_sec || (t0->tv_sec == t1->tv_sec && t0->tv_nsec < t1->tv_nsec)) ? true : false;
}

bool timespec_le(const struct timespec* _Nonnull t0, const struct timespec* _Nonnull t1)
{
    return (t0->tv_sec < t1->tv_sec || (t0->tv_sec == t1->tv_sec && t0->tv_nsec <= t1->tv_nsec)) ? true : false;
}

bool timespec_gt(const struct timespec* _Nonnull t0, const struct timespec* _Nonnull t1)
{
    return (t0->tv_sec > t1->tv_sec || (t0->tv_sec == t1->tv_sec && t0->tv_nsec > t1->tv_nsec)) ? true : false;
}

bool timespec_ge(const struct timespec* _Nonnull t0, const struct timespec* _Nonnull t1)
{
    return (t0->tv_sec > t1->tv_sec || (t0->tv_sec == t1->tv_sec && t0->tv_nsec >= t1->tv_nsec)) ? true : false;
}


void timespec_add(const struct timespec* _Nonnull t0, const struct timespec* _Nonnull t1, struct timespec* _Nonnull res)
{
    res->tv_sec = t0->tv_sec + t1->tv_sec;
    res->tv_nsec = t0->tv_nsec + t1->tv_nsec;

    if (res->tv_nsec >= NSEC_PER_SEC) {
        res->tv_sec++;
        res->tv_nsec -= NSEC_PER_SEC;
    }
    
    // Check for overflow
    if (res->tv_sec < 0) {
        *res = TIMESPEC_INF;
    }
}

void timespec_sub(const struct timespec* _Nonnull t0, const struct timespec* _Nonnull t1, struct timespec* _Nonnull res)
{
    res->tv_sec = t0->tv_sec - t1->tv_sec;
    res->tv_nsec = t0->tv_nsec - t1->tv_nsec;
    
    if (res->tv_nsec < 0) {
        res->tv_sec--;
        res->tv_nsec += NSEC_PER_SEC;
    }

    // Check for underflow
    if (res->tv_sec < 0) {
        *res = TIMESPEC_ZERO;
    }
}


void timespec_normalize(struct timespec* _Nonnull ts)
{
    if (ts->tv_sec < 0 || (ts->tv_sec == 0 && ts->tv_nsec < 0)) {
        // Input represents an underflow
        *ts = TIMESPEC_ZERO;
        return;
    }


    // Handle overflow in nanoseconds
    while(ts->tv_nsec >= NSEC_PER_SEC) {
		ts->tv_sec++;
		ts->tv_nsec -= NSEC_PER_SEC;
	}
	
    if (ts->tv_sec < 0) {
        // Previous loop overflowed
        *ts = TIMESPEC_INF;
        return;
    }


    // Handle underflow in nanoseconds
	while(ts->tv_nsec < 0) {
		ts->tv_sec--;
		ts->tv_nsec += NSEC_PER_SEC;
	}

    if (ts->tv_sec < 0) {
        // Previous loop underflowed
        *ts = TIMESPEC_ZERO;
    }
}
