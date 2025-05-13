//
//  Clock.c
//  libsystem
//
//  Created by Dietmar Planitzer on 2/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include <System/Clock.h>
#include <System/_syscall.h>


errno_t clock_wait(clockid_t clock, const TimeInterval* _Nonnull delay)
{
    return (errno_t)_syscall(SC_clock_wait, clock, delay);
}

errno_t clock_gettime(clockid_t clock, TimeInterval* _Nonnull ts)
{
    return (errno_t)_syscall(SC_clock_gettime, clock, ts);
}
