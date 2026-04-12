//
//  clock.h
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef _SERENA_CLOCK_H
#define _SERENA_CLOCK_H 1

#include <_cmndef.h>
#include <_null.h>
#include <kpi/clock.h>

__CPP_BEGIN

// Returns the current time of the monotonic clock. The monotonic clock starts
// ticking at boot time and never moves backward.
// @Concurrency: Safe
extern int clock_time(clockid_t clockid, struct timespec* _Nonnull ts);

// Returns information about the clock 'clockid'.
extern int clock_info(clockid_t clockid, int flavor, clock_info_ref _Nonnull info);

// Suspends the calling execution context for the seconds and nanoseconds specified
// by 'wtp'. 'wtp' is an absolute point in time if 'flags' includes TIMER_ABSTIME.
// Otherwise 'wtp' is a duration relative to the current time. 'rmtp' is set to
// the remaining time if this function wakes up before the specified timeout
// value.
// @Concurrency: Safe
extern int clock_wait(clockid_t clockid, int flags, const struct timespec* _Nonnull wtp, struct timespec* _Nullable rmtp);

__CPP_END

#endif /* _SERENA_CLOCK_H */
