//
//  Process.c
//  libsystem
//
//  Created by Dietmar Planitzer on 2/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include <System/Process.h>
#include <System/_syscall.h>


_Noreturn os_exit(int exit_code)
{
    _syscall(SC_exit, exit_code);
}


FilePermissions getumask(void)
{
    return _syscall(SC_getumask);
}

void setumask(FilePermissions mask)
{
    _syscall(SC_setumask, mask);
}


errno_t os_spawn(const char* _Nonnull path, const char* _Nullable argv[], const spawn_opts_t* _Nullable options, pid_t* _Nullable rpid)
{
    return _syscall(SC_spawn, path, argv, options, rpid);
}

pargs_t* _Nonnull getpargs(void)
{
    return (pargs_t*) _syscall(SC_getpargs);
}


errno_t vm_alloc(size_t nbytes, void* _Nullable * _Nonnull ptr)
{
    return _syscall(SC_vmalloc, nbytes, ptr);
}
