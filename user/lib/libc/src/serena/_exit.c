//
//  _exit.c
//  libc
//
//  Created by Dietmar Planitzer on 5/15/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <kpi/syscall.h>
#include <serena/process.h>


_Noreturn void _exit(int status)
{
    (void)_syscall(SC_exit, status);
}

int proc_exec(const char* _Nonnull path, const char* _Nullable argv[], const char* _Nullable * _Nullable envp)
{
    return (int)_syscall(SC_proc_exec, path, argv, envp);
}
