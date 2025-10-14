//
//  clock_gettime.c
//  libc
//
//  Created by Dietmar Planitzer on 5/15/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <time.h>
#include <kpi/syscall.h>


int clock_gettime(clockid_t clockid, struct timespec* _Nonnull ts)
{
    return (int)_syscall(SC_clock_gettime, clockid, ts);
}
