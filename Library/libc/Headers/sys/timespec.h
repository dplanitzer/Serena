//
//  sys/timespec.h
//  libc
//
//  Created by Dietmar Planitzer on 5/15/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _SYS_TIMESPEC_H
#define _SYS_TIMESPEC_H

#include <System/_cmndef.h>
#include <System/_time.h>
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


inline struct timespec timespec_from(time_t seconds, long nanoseconds) {
    struct timespec ts;
    ts.tv_sec = seconds;
    ts.tv_nsec = nanoseconds;
    return ts;
}

inline struct timespec timespec_from_sec(time_t seconds) {
    struct timespec ts;
    ts.tv_sec = seconds;
    ts.tv_nsec = 0l;
    return ts;
}

inline struct timespec timespec_from_ms(mseconds_t millis) {
    struct timespec ts;
    ts.tv_sec = millis / 1000l;
    ts.tv_nsec = (millis - (ts.tv_sec * 1000l)) * 1000l * 1000l;
    return ts;
}

inline struct timespec timespec_from_us(useconds_t micros) {
    struct timespec ts;
    ts.tv_sec = micros / (1000l * 1000l);
    ts.tv_nsec = (micros - (ts.tv_sec * 1000l * 1000l)) * 1000l;
    return ts;
}


inline time_t timespec_sec(struct timespec ts) {
    return ts.tv_sec;
}

inline mseconds_t timespec_ms(struct timespec ts) {
    return ts.tv_sec * 1000l + ts.tv_nsec / (1000l * 1000l);
}

inline useconds_t timespec_us(struct timespec ts) {
    return ts.tv_sec * 1000l * 1000l + ts.tv_nsec / 1000l;
}

inline int64_t timespec_ns(struct timespec ts) {
    return (int64_t)ts.tv_sec * 1000ll * 1000ll * 1000ll + (int64_t)ts.tv_nsec;
}


inline bool timespec_isneg(struct timespec ts) {
    return ts.tv_sec < 0 || ts.tv_nsec < 0;
}


inline bool timespec_eq(struct timespec t0, struct timespec t1) {
    return t0.tv_nsec == t1.tv_nsec && t0.tv_sec == t1.tv_sec;
}

inline bool timespec_ls(struct timespec t0, struct timespec t1) {
    return (t0.tv_sec < t1.tv_sec || (t0.tv_sec == t1.tv_sec && t0.tv_nsec < t1.tv_nsec));
}

inline bool timespec_lsq(struct timespec t0, struct timespec t1) {
    return (t0.tv_sec < t1.tv_sec || (t0.tv_sec == t1.tv_sec && t0.tv_nsec <= t1.tv_nsec));
}

inline bool timespec_gt(struct timespec t0, struct timespec t1) {
    return (t0.tv_sec > t1.tv_sec || (t0.tv_sec == t1.tv_sec && t0.tv_nsec > t1.tv_nsec));
}

inline bool timespec_gtq(struct timespec t0, struct timespec t1) {
    return (t0.tv_sec > t1.tv_sec || (t0.tv_sec == t1.tv_sec && t0.tv_nsec >= t1.tv_nsec));
}


extern struct timespec timespec_add(struct timespec t0, struct timespec t1);

extern struct timespec timespec_sub(struct timespec t0, struct timespec t1);


extern const struct timespec    TIMESPEC_ZERO;
extern const struct timespec    TIMESPEC_INF;
extern const struct timespec    TIMESPEC_NEGINF;

#endif /* _SYS_TIMESPEC_H */
