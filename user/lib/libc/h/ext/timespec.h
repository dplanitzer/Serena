//
//  ext/timespec.h
//  libc, libsc
//
//  Created by Dietmar Planitzer on 5/15/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _EXT_TIMESPEC_H
#define _EXT_TIMESPEC_H

#include <_cmndef.h>
#include <stdint.h>
#include <kpi/_time.h>


// 'struct timespec' represents time as measured in seconds and nanoseconds.
// All functions expect timespec inputs in normalized form - meaning the
// seconds field is in the range [0..LONGMAX] and the nanoseconds field is in
// the range [0..1000_000_000).
//
// Timespec is a saturating type. This means that a time value is set to
// +/-infinity on overflow/underflow.
//
// Note that all timespec functions assume that they receive a valid timespec as
// input. The only exception is timespec_normalize() which you can use to
// convert a valid or non-valid timespec into a valid timespec.


#define timespec_from(__ts, __seconds, __nanoseconds) \
(__ts)->tv_sec = (__seconds); \
(__ts)->tv_nsec = (__nanoseconds)

#define timespec_from_sec(__ts, __seconds) \
(__ts)->tv_sec = (__seconds); \
(__ts)->tv_nsec = 0l

extern void timespec_from_ms(struct timespec* _Nonnull ts, mseconds_t millis);
extern void timespec_from_us(struct timespec* _Nonnull ts, useconds_t micros);


#define timespec_sec(__ts) \
(__ts)->tv_sec

extern mseconds_t timespec_ms(struct timespec* _Nonnull ts);
extern useconds_t timespec_us(struct timespec* _Nonnull ts);
extern int64_t timespec_ns(struct timespec* _Nonnull ts);


#define timespec_isset(__ts) \
((__ts)->tv_sec != 0 || (__ts)->tv_nsec != 0)

#define timespec_isvalid(__ts) \
((__ts)->tv_sec >=0 && (__ts)->tv_nsec < NSEC_PER_SEC)


#define timespec_clear(__ts) \
(__ts)->tv_sec = 0; \
(__ts)->tv_nsec = 0


#define timespec_eq(__t0, __t1) \
((__t0)->tv_sec == (__t1)->tv_sec && (__t0)->tv_nsec == (__t1)->tv_nsec)

#define timespec_lt(__t0, __t1) \
((__t0)->tv_sec < (__t1)->tv_sec || ((__t0)->tv_sec == (__t1)->tv_sec && (__t0)->tv_nsec < (__t1)->tv_nsec))

#define timespec_le(__t0, __t1) \
((__t0)->tv_sec < (__t1)->tv_sec || ((__t0)->tv_sec == (__t1)->tv_sec && (__t0)->tv_nsec <= (__t1)->tv_nsec))

#define timespec_gt(__t0, __t1) \
((__t0)->tv_sec > (__t1)->tv_sec || ((__t0)->tv_sec == (__t1)->tv_sec && (__t0)->tv_nsec > (__t1)->tv_nsec))

#define timespec_ge(__t0, __t1) \
((__t0)->tv_sec > (__t1)->tv_sec || ((__t0)->tv_sec == (__t1)->tv_sec && (__t0)->tv_nsec >= (__t1)->tv_nsec))


extern void timespec_add(const struct timespec* _Nonnull t0, const struct timespec* _Nonnull t1, struct timespec* _Nonnull res);
extern void timespec_sub(const struct timespec* _Nonnull t0, const struct timespec* _Nonnull t1, struct timespec* _Nonnull res);


// Normalizes the given timespec in the sense that it accepts a timespec for
// which timespec_isvalid() returns false and it adjusts the timespec such that
// timespec_isvalid() will return true on it.
extern void timespec_normalize(struct timespec* _Nonnull ts);


extern const struct timespec    TIMESPEC_ZERO;
extern const struct timespec    TIMESPEC_INF;

#endif /* _EXT_TIMESPEC_H */
