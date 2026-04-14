//
//  ext/nanotime.c
//  libc, libsc
//
//  Created by Dietmar Planitzer on 2/12/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include <ext/nanotime.h>
#include <limits.h>


const nanotime_t   NANOTIME_ZERO = {0l, 0l};
const nanotime_t   NANOTIME_INF = {LONG_MAX, NSEC_PER_SEC-1l};


void nanotime_from_ms(nanotime_t* _Nonnull ts, mseconds_t millis)
{
    ts->tv_sec = millis / MSEC_PER_SEC;
    ts->tv_nsec = (millis - (ts->tv_sec * MSEC_PER_SEC)) * 1000000l;
}

void nanotime_from_us(nanotime_t* _Nonnull ts, useconds_t micros)
{
    ts->tv_sec = micros / USEC_PER_SEC;
    ts->tv_nsec = (micros - (ts->tv_sec * USEC_PER_SEC)) * 1000l;
}


mseconds_t nanotime_ms(nanotime_t* _Nonnull ts)
{
    return ts->tv_sec * MSEC_PER_SEC + ts->tv_nsec / 1000000l;
}

useconds_t nanotime_us(nanotime_t* _Nonnull ts)
{
    return ts->tv_sec * USEC_PER_SEC + ts->tv_nsec / 1000l;
}

int64_t nanotime_ns(nanotime_t* _Nonnull ts)
{
    return (int64_t)ts->tv_sec * (int64_t)NSEC_PER_SEC + (int64_t)ts->tv_nsec;
}


void nanotime_add(const nanotime_t* _Nonnull t0, const nanotime_t* _Nonnull t1, nanotime_t* _Nonnull res)
{
    res->tv_sec = t0->tv_sec + t1->tv_sec;
    res->tv_nsec = t0->tv_nsec + t1->tv_nsec;

    if (res->tv_nsec >= NSEC_PER_SEC) {
        res->tv_sec++;
        res->tv_nsec -= NSEC_PER_SEC;
    }
    
    // Check for overflow
    if (res->tv_sec < 0) {
        *res = NANOTIME_INF;
    }
}

void nanotime_sub(const nanotime_t* _Nonnull t0, const nanotime_t* _Nonnull t1, nanotime_t* _Nonnull res)
{
    res->tv_sec = t0->tv_sec - t1->tv_sec;
    res->tv_nsec = t0->tv_nsec - t1->tv_nsec;
    
    if (res->tv_nsec < 0) {
        res->tv_sec--;
        res->tv_nsec += NSEC_PER_SEC;
    }

    // Check for underflow
    if (res->tv_sec < 0) {
        *res = NANOTIME_ZERO;
    }
}


void nanotime_normalize(nanotime_t* _Nonnull ts)
{
    if (ts->tv_sec < 0 || (ts->tv_sec == 0 && ts->tv_nsec < 0)) {
        // Input represents an underflow
        *ts = NANOTIME_ZERO;
        return;
    }


    // Handle overflow in nanoseconds
    while(ts->tv_nsec >= NSEC_PER_SEC) {
		ts->tv_sec++;
		ts->tv_nsec -= NSEC_PER_SEC;
	}
	
    if (ts->tv_sec < 0) {
        // Previous loop overflowed
        *ts = NANOTIME_INF;
        return;
    }


    // Handle underflow in nanoseconds
	while(ts->tv_nsec < 0) {
		ts->tv_sec--;
		ts->tv_nsec += NSEC_PER_SEC;
	}

    if (ts->tv_sec < 0) {
        // Previous loop underflowed
        *ts = NANOTIME_ZERO;
    }
}
