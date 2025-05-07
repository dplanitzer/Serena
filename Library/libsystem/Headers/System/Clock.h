//
//  Clock.h
//  libsystem
//
//  Created by Dietmar Planitzer on 2/23/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef _SYS_CLOCK_H
#define _SYS_CLOCK_H 1

#include <System/_cmndef.h>
#include <System/_noreturn.h>
#include <System/Error.h>
#include <System/TimeInterval.h>
#include <System/Types.h>

__CPP_BEGIN

// Time counted since the system was booted. This is a monotonic clock that does
// not undergo adjustments to keep it aligned with a wall time reference clock.
#define CLOCK_UPTIME    0


// Blocks the calling execution context for the seconds and nanoseconds specified
// by 'delay'.
// @Concurrency: Safe
extern errno_t clock_wait(int clock, const TimeInterval* _Nonnull delay);

// Returns the current time of the monotonic clock. The monotonic clock starts
// ticking at boot time and never moves backward.
// @Concurrency: Safe
extern errno_t clock_gettime(int clock, TimeInterval* _Nonnull ts);

__CPP_END

#endif /* _SYS_CLOCK_H */
