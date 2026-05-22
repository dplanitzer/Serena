//
//  proc_wait.c
//  libc
//
//  Created by Dietmar Planitzer on 5/14/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <serena/proc_wait.h>
#include <kpi/syscall.h>

int proc_wait(int wstate, int match, pid_t id, int flags, proc_waitres_t* _Nonnull res)
{
    return (int)_syscall(SC_proc_wait, wstate, match, id, flags, res);
}
