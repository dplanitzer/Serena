//
//  spawn.c
//  libc
//
//  Created by Dietmar Planitzer on 5/15/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <sys/spawn.h>
#include <sys/_syscall.h>
#include <System/_varargs.h>


int os_spawn(const char* _Nonnull path, const char* _Nullable argv[], const spawn_opts_t* _Nullable options, pid_t* _Nullable rpid)
{
    return (int)_syscall(SC_spawn, path, argv, options, rpid);
}
