//
//  proc_schedparam.c
//  libc
//
//  Created by Dietmar Planitzer on 3/24/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include <serena/process.h>
#include <kpi/syscall.h>


int proc_schedparam(pid_t pid, int type, int* _Nonnull param)
{
    return (int)_syscall(SC_proc_schedparam, pid, type, param);
}

int proc_setschedparam(pid_t pid, int type, const int* _Nonnull param)
{
    return (int)_syscall(SC_proc_setschedparam, pid, type, param);
}
