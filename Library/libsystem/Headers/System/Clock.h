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

#if !defined(__KERNEL__)

// Blocks the calling execution context for teh seconds and nanoseconds specified
// by 'delay'.
// @Concurrency: Safe
extern errno_t Delay(TimeInterval delay);

// Returns the current time of the monotonic clock. The monotonic clock starts
// ticking at boot time and never moves backward.
// @Concurrency: Safe
extern TimeInterval MonotonicClock_GetTime(void);

#endif /* __KERNEL__ */

__CPP_END

#endif /* _SYS_CLOCK_H */
