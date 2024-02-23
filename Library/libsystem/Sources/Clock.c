//
//  Clock.c
//  libsystem
//
//  Created by Dietmar Planitzer on 2/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include <System/Clock.h>
#include <System/_syscall.h>


errno_t Delay(TimeInterval ti)
{
    return (errno_t)_syscall(SC_delay, ti);
}

TimeInterval MonotonicClock_GetTime(void)
{
    TimeInterval time;

    _syscall(SC_get_monotonic_time, &time);
    return time;
}
