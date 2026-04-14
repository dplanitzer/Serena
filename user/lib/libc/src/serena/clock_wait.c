//
//  clock_wait.c
//  libc
//
//  Created by Dietmar Planitzer on 5/15/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <serena/clock.h>
#include <kpi/syscall.h>


int clock_wait(clockid_t clockid, int flags, const nanotime_t* _Nonnull wtp, nanotime_t* _Nullable rmtp)
{
    return (int)_syscall(SC_clock_wait, clockid, flags, wtp, rmtp);
}
