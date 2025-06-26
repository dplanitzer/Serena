//
//  sched_yield.c
//  libc
//
//  Created by Dietmar Planitzer on 6/25/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <sched.h>
#include <sys/errno.h>
#include <kpi/syscall.h>


void sched_yield(void)
{
    (void)_syscall(SC_sched_yield);
}
