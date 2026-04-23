//
//  proc_spawn.c
//  libc
//
//  Created by Dietmar Planitzer on 5/15/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <serena/process.h>
#include <kpi/syscall.h>
#include <__readdir.h>


int proc_spawn(dir_t _Nullable wd, const char* _Nonnull path, const char* _Nullable argv[], const proc_spawn_t* _Nullable options, pid_t* _Nullable rpid)
{
    return (int)_syscall(SC_proc_spawn, (wd) ? _dir_fd(wd) : FD_CWD, path, argv, options, rpid);
}
