//
//  ext/nanotime.h
//  libc, libsc
//
//  Created by Dietmar Planitzer on 5/15/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _EXT_NANOTIME_H
#define _EXT_NANOTIME_H

#include <_cmndef.h>
#include <stdint.h>
#include <kpi/_time.h>


// 'nanotime_t' represents time as measured in seconds and nanoseconds.
// All functions expect nanotime inputs in normalized form - meaning the
// seconds field is in the range [0..LONGMAX] and the nanoseconds field is in
// the range [0..1000_000_000).
//
// Timespec is a saturating type. This means that a time value is set to
// +/-infinity on overflow/underflow.
//
// Note that all nanotime functions assume that they receive a valid nanotime as
// input. The only exception is nanotime_normalize() which you can use to
// convert a valid or non-valid nanotime into a valid nanotime.


#define nanotime_from(__ts, __seconds, __nanoseconds) \
(__ts)->tv_sec = (__seconds); \
(__ts)->tv_nsec = (__nanoseconds)

#define nanotime_from_sec(__ts, __seconds) \
(__ts)->tv_sec = (__seconds); \
(__ts)->tv_nsec = 0l

extern void nanotime_from_ms(nanotime_t* _Nonnull ts, mseconds_t millis);
extern void nanotime_from_us(nanotime_t* _Nonnull ts, useconds_t micros);


extern mseconds_t nanotime_ms(nanotime_t* _Nonnull ts);
extern useconds_t nanotime_us(nanotime_t* _Nonnull ts);
extern int64_t nanotime_ns(nanotime_t* _Nonnull ts);


#define nanotime_isset(__ts) \
((__ts)->tv_sec != 0 || (__ts)->tv_nsec != 0)

#define nanotime_isvalid(__ts) \
((__ts)->tv_sec >=0 && (__ts)->tv_nsec < NSEC_PER_SEC)


#define nanotime_clear(__ts) \
(__ts)->tv_sec = 0; \
(__ts)->tv_nsec = 0


#define nanotime_eq(__t0, __t1) \
((__t0)->tv_sec == (__t1)->tv_sec && (__t0)->tv_nsec == (__t1)->tv_nsec)

#define nanotime_lt(__t0, __t1) \
((__t0)->tv_sec < (__t1)->tv_sec || ((__t0)->tv_sec == (__t1)->tv_sec && (__t0)->tv_nsec < (__t1)->tv_nsec))

#define nanotime_le(__t0, __t1) \
((__t0)->tv_sec < (__t1)->tv_sec || ((__t0)->tv_sec == (__t1)->tv_sec && (__t0)->tv_nsec <= (__t1)->tv_nsec))

#define nanotime_gt(__t0, __t1) \
((__t0)->tv_sec > (__t1)->tv_sec || ((__t0)->tv_sec == (__t1)->tv_sec && (__t0)->tv_nsec > (__t1)->tv_nsec))

#define nanotime_ge(__t0, __t1) \
((__t0)->tv_sec > (__t1)->tv_sec || ((__t0)->tv_sec == (__t1)->tv_sec && (__t0)->tv_nsec >= (__t1)->tv_nsec))


extern void nanotime_add(const nanotime_t* _Nonnull t0, const nanotime_t* _Nonnull t1, nanotime_t* _Nonnull res);
extern void nanotime_sub(const nanotime_t* _Nonnull t0, const nanotime_t* _Nonnull t1, nanotime_t* _Nonnull res);


// Normalizes the given nanotime in the sense that it accepts a nanotime for
// which nanotime_isvalid() returns false and it adjusts the nanotime such that
// nanotime_isvalid() will return true on it.
extern void nanotime_normalize(nanotime_t* _Nonnull ts);


extern const nanotime_t NANOTIME_ZERO;
extern const nanotime_t NANOTIME_INF;

#endif /* _EXT_NANOTIME_H */
