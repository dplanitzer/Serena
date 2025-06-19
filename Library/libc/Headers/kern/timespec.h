//
//  kern/timespec.h
//  libc
//
//  Created by Dietmar Planitzer on 5/15/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _KERN_TIMESPEC_H
#define _KERN_TIMESPEC_H

#include <_cmndef.h>
#include <kpi/_time.h>
#include <stdbool.h>
#include <stdint.h>



// 'struct timespec' represents time as measured in seconds and nanoseconds.
// All functions expect timespec inputs in canonical form - meaning the
// nanoseconds field is in the range [0..1000_000_000). Negative timespec
// values are represented with a negative seconds field if seconds != 0 and a
// negative nanoseconds field if seconds == 0 and nanoseconds != 0.
//
// Timespec is a saturating type. This means that a time value is set to
// +/-infinity on overflow/underflow.


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


#define timespec_isneg(__ts) \
((__ts)->tv_sec < 0 || (__ts)->tv_nsec < 0)


extern bool timespec_eq(const struct timespec* _Nonnull t0, const struct timespec* _Nonnull t1);
extern bool timespec_lt(const struct timespec* _Nonnull t0, const struct timespec* _Nonnull t1);
extern bool timespec_le(const struct timespec* _Nonnull t0, const struct timespec* _Nonnull t1);
extern bool timespec_gt(const struct timespec* _Nonnull t0, const struct timespec* _Nonnull t1);
extern bool timespec_ge(const struct timespec* _Nonnull t0, const struct timespec* _Nonnull t1);


extern void timespec_add(const struct timespec* _Nonnull t0, const struct timespec* _Nonnull t1, struct timespec* _Nonnull res);
extern void timespec_sub(const struct timespec* _Nonnull t0, const struct timespec* _Nonnull t1, struct timespec* _Nonnull res);


extern const struct timespec    TIMESPEC_ZERO;
extern const struct timespec    TIMESPEC_INF;
extern const struct timespec    TIMESPEC_NEGINF;

#endif /* _KERN_TIMESPEC_H */
