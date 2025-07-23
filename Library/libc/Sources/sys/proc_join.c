//
//  proc_join.c
//  libc
//
//  Created by Dietmar Planitzer on 5/14/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <sys/wait.h>
#include <kpi/syscall.h>


int proc_join(int scope, pid_t id, struct proc_status* _Nonnull ps)
{
    return proc_timedjoin(scope, id, TIMER_ABSTIME, &TIMESPEC_INF, ps);
}

int proc_timedjoin(int scope, pid_t id, int flags, const struct timespec* _Nonnull wtp, struct proc_status* _Nonnull ps)
{
    return (int)_syscall(SC_proc_join, scope, id, flags, wtp, ps);
}
