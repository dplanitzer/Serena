//
//  clock_time.c
//  libc
//
//  Created by Dietmar Planitzer on 5/15/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <kpi/syscall.h>
#include <serena/clock.h>

int clock_time(clockid_t clockid, nanotime_t* _Nonnull ts)
{
    return (int)_syscall(SC_clock_time, clockid, ts);
}
