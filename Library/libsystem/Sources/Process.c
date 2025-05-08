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

errno_t os_getcwd(char* _Nonnull buffer, size_t bufferSize)
{
    return _syscall(SC_getcwd, buffer, bufferSize);
}

errno_t os_setcwd(const char* _Nonnull path)
{
    return _syscall(SC_setcwd, path);
}


FilePermissions os_getumask(void)
{
    return _syscall(SC_getumask);
}

void os_setumask(FilePermissions mask)
{
    _syscall(SC_setumask, mask);
}

pid_t os_getpid(void)
{
    return _syscall(SC_getpid);
}

pid_t os_getppid(void)
{
    return _syscall(SC_getppid);
}

uid_t os_getuid(void)
{
    return _syscall(SC_getuid);
}

gid_t os_getgid(void)
{
    return _syscall(SC_getgid);
}

errno_t os_spawn(const char* _Nonnull path, const char* _Nullable argv[], const os_spawn_opts_t* _Nullable options, pid_t* _Nullable rpid)
{
    return _syscall(SC_spawn, path, argv, options, rpid);
}

errno_t os_waitpid(pid_t pid, os_pstatus_t* _Nullable result)
{
    return _syscall(SC_waitpid, pid, result);
}

os_pargs_t* _Nonnull os_getpargs(void)
{
    return (os_pargs_t*) _syscall(SC_getpargs);
}


errno_t os_vmalloc(size_t nbytes, void* _Nullable * _Nonnull ptr)
{
    return _syscall(SC_vmalloc, nbytes, ptr);
}
