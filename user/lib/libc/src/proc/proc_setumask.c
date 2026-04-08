//
//  proc_setumask.c
//  libc
//
//  Created by Dietmar Planitzer on 5/15/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <serena/process.h>
#include <kpi/syscall.h>

void proc_setumask(mode_t mask)
{
    (void)_syscall(SC_proc_setumask, mask);
}
