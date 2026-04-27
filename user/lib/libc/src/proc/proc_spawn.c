//
//  proc_spawn.c
//  libc
//
//  Created by Dietmar Planitzer on 5/15/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <serena/spawn.h>
#include <kpi/syscall.h>


int proc_spawn(const char* _Nonnull path, const char* _Nullable argv[], const char* _Nullable envp[], const proc_spawnattr_t* _Nonnull attr, const proc_spawn_actions_t* _Nullable actions, proc_spawnres_t* _Nullable result)
{
    return (int)_syscall(SC_proc_spawn, path, argv, envp, attr, actions, result);
}
