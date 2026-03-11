//
//  clock.h
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef _SYS_CLOCK_H
#define _SYS_CLOCK_H 1

#include <_cmndef.h>
#include <arch/_null.h>
#include <kpi/_time.h>

__CPP_BEGIN

// Returns the current time of the monotonic clock. The monotonic clock starts
// ticking at boot time and never moves backward.
// @Concurrency: Safe
extern int clock_gettime(clockid_t clockid, struct timespec* _Nonnull ts);

// Returns the resolution of the clock 'clockid'. The resolution is the duration
// of a single clock tick in terms of seconds and nanoseconds.
extern int clock_getres(clockid_t clockid, struct timespec* _Nonnull res);

// Suspends the calling execution context for the seconds and nanoseconds specified
// by 'wtp'. 'wtp' is an absolute point in time if 'flags' includes TIMER_ABSTIME.
// Otherwise 'wtp' is a duration relative to the current time. 'rmtp' is set to
// the remaining time if this function wakes up before the specified timeout
// value.
// @Concurrency: Safe
extern int clock_nanosleep(clockid_t clockid, int flags, const struct timespec* _Nonnull wtp, struct timespec* _Nullable rmtp);


// Suspends the calling execution context for the specified time. The time may
// be specified with a resolution of nanoseconds. Returns the amount of unslept
// time if the function is interrupted for some reason. Returns 0 on success and
// -1 on failure or if woken up early.
extern int nanosleep(const struct timespec* _Nonnull wtp, struct timespec* _Nullable rmtp);

__CPP_END

#endif /* _SYS_CLOCK_H */
