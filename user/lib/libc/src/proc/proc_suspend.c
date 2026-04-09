//
//  proc_suspend.c
//  libc
//
//  Created by Dietmar Planitzer on 4/8/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include <serena/process.h>
#include <kpi/syscall.h>


int proc_suspend(pid_t pid)
{
    return (int)_syscall(SC_proc_suspend, pid);
}
