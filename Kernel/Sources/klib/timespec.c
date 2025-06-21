//
//  timespec.c
//  klib
//
//  Created by Dietmar Planitzer on 2/12/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include <sys/timespec.h>
#include <limits.h>


#define ONE_SECOND_IN_NANOS (1000l * 1000l * 1000l)

#define timespec_isneg(__ts) \
((__ts)->tv_sec < 0 || (__ts)->tv_nsec < 0)


const struct timespec   TIMESPEC_ZERO = {0l, 0l};
const struct timespec   TIMESPEC_INF = {LONG_MAX, ONE_SECOND_IN_NANOS-1l};


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
    if (res->tv_nsec >= ONE_SECOND_IN_NANOS) {
        // handle carry
        res->tv_sec++;
        res->tv_nsec -= ONE_SECOND_IN_NANOS;
    }
    
    // Saturate on overflow
    // See Assembly Language and Systems Programming for the M68000 Family p41
    if ((t0->tv_sec >= 0 && t1->tv_sec >= 0 && res->tv_sec < 0)
        || (t0->tv_sec < 0 && t1->tv_sec < 0 && res->tv_sec >= 0)) {
        *res = (timespec_isneg(t0) && timespec_isneg(t1)) ? TIMESPEC_ZERO : TIMESPEC_INF;
    }
}

void timespec_sub(const struct timespec* _Nonnull t0, const struct timespec* _Nonnull t1, struct timespec* _Nonnull res)
{
    if (timespec_gt(t0, t1)) {
        // t0 > t1
        res->tv_sec = t0->tv_sec - t1->tv_sec;
        res->tv_nsec = t0->tv_nsec - t1->tv_nsec;
        if (res->tv_nsec < 0) {
            // handle borrow
            res->tv_nsec += ONE_SECOND_IN_NANOS;
            res->tv_sec--;
        }
    }
    else {
        // t0 <= t1 -> swap t0 and t1 and negate the result
        res->tv_sec = t1->tv_sec - t0->tv_sec;
        res->tv_nsec = t1->tv_nsec - t0->tv_nsec;
        if (res->tv_nsec < 0) {
            // handle borrow
            res->tv_nsec += ONE_SECOND_IN_NANOS;
            res->tv_sec--;
        }
        if (res->tv_sec != 0) {
            res->tv_sec = -res->tv_sec;
        } else {
            res->tv_nsec = -res->tv_nsec;
        }
    }

    // Saturate on overflow
    // See Assembly Language and Systems Programming for the M68000 Family p41
    if ((t0->tv_sec < 0 && t1->tv_sec >= 0 && res->tv_sec >= 0)
        || (t0->tv_sec >= 0 && t1->tv_sec < 0 && res->tv_sec < 0)) {
        *res = (timespec_isneg(t0) && timespec_isneg(t1)) ? TIMESPEC_ZERO : TIMESPEC_INF;
    }
}
