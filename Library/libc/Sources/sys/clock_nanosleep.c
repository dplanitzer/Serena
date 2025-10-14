//
//  clock_nanosleep.c
//  libc
//
//  Created by Dietmar Planitzer on 5/15/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <time.h>
#include <kpi/syscall.h>


int clock_nanosleep(clockid_t clockid, int flags, const struct timespec* _Nonnull wtp, struct timespec* _Nullable rmtp)
{
    return (int)_syscall(SC_clock_nanosleep, clockid, flags, wtp, rmtp);
}
