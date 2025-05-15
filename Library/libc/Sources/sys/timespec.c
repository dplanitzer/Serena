//
//  timespec.c
//  libc
//
//  Created by Dietmar Planitzer on 2/12/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include <sys/timespec.h>
#include <limits.h>
#define ONE_SECOND_IN_NANOS (1000l * 1000l * 1000l)


const struct timespec   TIMESPEC_ZERO = {0l, 0l};
const struct timespec   TIMESPEC_INF = {LONG_MAX, ONE_SECOND_IN_NANOS};
const struct timespec   TIMESPEC_NEGINF = {LONG_MIN, ONE_SECOND_IN_NANOS};

struct timespec timespec_add(struct timespec t0, struct timespec t1)
{
    struct timespec ts;
    
    ts.tv_sec = t0.tv_sec + t1.tv_sec;
    ts.tv_nsec = t0.tv_nsec + t1.tv_nsec;
    if (ts.tv_nsec >= ONE_SECOND_IN_NANOS) {
        // handle carry
        ts.tv_sec++;
        ts.tv_nsec -= ONE_SECOND_IN_NANOS;
    }
    
    // Saturate on overflow
    // See Assembly Language and Systems Programming for the M68000 Family p41
    if ((t0.tv_sec >= 0 && t1.tv_sec >= 0 && ts.tv_sec < 0) || (t0.tv_sec < 0 && t1.tv_sec < 0 && ts.tv_sec >= 0)) {
        ts = (timespec_isneg(t0) && timespec_isneg(t1)) ? TIMESPEC_NEGINF : TIMESPEC_INF;
    }
    
    return ts;
}

struct timespec timespec_sub(struct timespec t0, struct timespec t1)
{
    struct timespec ts;
    
    if (timespec_gt(t0, t1)) {
        // t0 > t1
        ts.tv_sec = t0.tv_sec - t1.tv_sec;
        ts.tv_nsec = t0.tv_nsec - t1.tv_nsec;
        if (ts.tv_nsec < 0) {
            // handle borrow
            ts.tv_nsec += ONE_SECOND_IN_NANOS;
            ts.tv_sec--;
        }
    } else {
        // t0 <= t1 -> swap t0 and t1 and negate the result
        ts.tv_sec = t1.tv_sec - t0.tv_sec;
        ts.tv_nsec = t1.tv_nsec - t0.tv_nsec;
        if (ts.tv_nsec < 0) {
            // handle borrow
            ts.tv_nsec += ONE_SECOND_IN_NANOS;
            ts.tv_sec--;
        }
        if (ts.tv_sec != 0) {
            ts.tv_sec = -ts.tv_sec;
        } else {
            ts.tv_nsec = -ts.tv_nsec;
        }
    }

    // Saturate on overflow
    // See Assembly Language and Systems Programming for the M68000 Family p41
    if ((t0.tv_sec < 0 && t1.tv_sec >= 0 && ts.tv_sec >= 0) || (t0.tv_sec >= 0 && t1.tv_sec < 0 && ts.tv_sec < 0)) {
        ts = (timespec_isneg(t0) && timespec_isneg(t1)) ? TIMESPEC_NEGINF : TIMESPEC_INF;
    }
    
    return ts;
}
