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

errno_t getcwd(char* _Nonnull buffer, size_t bufferSize)
{
    return _syscall(SC_getcwd, buffer, bufferSize);
}


FilePermissions getumask(void)
{
    return _syscall(SC_getumask);
}

void setumask(FilePermissions mask)
{
    _syscall(SC_setumask, mask);
}

pid_t getpid(void)
{
    return _syscall(SC_getpid);
}

pid_t getppid(void)
{
    return _syscall(SC_getppid);
}

uid_t getuid(void)
{
    return _syscall(SC_getuid);
}

gid_t getgid(void)
{
    return _syscall(SC_getgid);
}

errno_t os_spawn(const char* _Nonnull path, const char* _Nullable argv[], const spawn_opts_t* _Nullable options, pid_t* _Nullable rpid)
{
    return _syscall(SC_spawn, path, argv, options, rpid);
}

errno_t waitpid(pid_t pid, pstatus_t* _Nullable result)
{
    return _syscall(SC_waitpid, pid, result);
}

pargs_t* _Nonnull getpargs(void)
{
    return (pargs_t*) _syscall(SC_getpargs);
}


errno_t vm_alloc(size_t nbytes, void* _Nullable * _Nonnull ptr)
{
    return _syscall(SC_vmalloc, nbytes, ptr);
}
