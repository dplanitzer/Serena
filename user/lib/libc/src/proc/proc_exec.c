//
//  proc_exec.c
//  libc
//
//  Created by Dietmar Planitzer on 5/15/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <kpi/syscall.h>
#include <serena/process.h>
#include <__readdir.h>


int proc_exec(dir_t _Nullable wd, const char* _Nonnull path, const char* _Nullable argv[], const char* _Nullable * _Nullable envp)
{
    return (int)_syscall(SC_proc_exec, (wd) ? _dir_fd(wd) : FD_CWD, path, argv, envp);
}
