//
//  clock_getres.c
//  libc
//
//  Created by Dietmar Planitzer on 10/13/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <time.h>
#include <kpi/syscall.h>


int clock_res(clockid_t clockid, struct timespec* _Nonnull res)
{
    return (int)_syscall(SC_clock_getres, clockid, res);
}
