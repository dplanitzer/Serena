//
//  clock_info.c
//  libc
//
//  Created by Dietmar Planitzer on 10/13/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <serena/clock.h>
#include <kpi/syscall.h>


int clock_info(clockid_t clockid, int flavor, clock_info_ref _Nonnull info)
{
    return (int)_syscall(SC_clock_info, clockid, flavor, info);
}
