//
//  proc_exit.c
//  libc
//
//  Created by Dietmar Planitzer on 5/15/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <kpi/syscall.h>
#include <serena/process.h>


_Noreturn void proc_exit(int status)
{
    (void)_syscall(SC_proc_exit, status);
}
