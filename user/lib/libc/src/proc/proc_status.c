//
//  proc_status.c
//  libc
//
//  Created by Dietmar Planitzer on 5/14/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <serena/process.h>
#include <kpi/syscall.h>

int proc_status(int of, pid_t id, int flags, proc_status_t* _Nonnull ps)
{
    return (int)_syscall(SC_proc_status, of, id, flags, ps);
}
