//
//  Process.c
//  libsystem
//
//  Created by Dietmar Planitzer on 2/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include <System/Process.h>
#include <System/_syscall.h>


_Noreturn Process_Exit(int exit_code)
{
    _syscall(SC_exit, exit_code);
}

errno_t Process_GetCurrentWorkingDirectoryPath(char* buffer, size_t bufferSize)
{
    return (errno_t)_syscall(SC_getcwd, buffer, bufferSize);
}

errno_t Process_SetCurrentWorkingDirectoryPath(const char* path)
{
    return (errno_t)_syscall(SC_setcwd, path);
}


FilePermissions Process_GetUserMask(void)
{
    return _syscall(SC_getumask);
}

void Process_SetUserMask(FilePermissions mask)
{
    _syscall(SC_setumask, mask);
}

ProcessId Process_GetId(void)
{
    return _syscall(SC_getpid);
}

ProcessId Process_GetParentId(void)
{
    return _syscall(SC_getppid);
}

UserId Process_GetUserId(void)
{
    return _syscall(SC_getuid);
}

errno_t Process_Spawn(const SpawnArguments *args, ProcessId *rpid)
{
    SpawnArguments kargs = *args;

    return (errno_t)_syscall(SC_spawn_process, &kargs, rpid);
}

errno_t Process_WaitForTerminationOfChild(ProcessId pid, ProcessTerminationStatus *result)
{
    return (errno_t)_syscall(SC_waitpid, pid, result);
}

ProcessArguments *Process_GetArguments(void)
{
    return (ProcessArguments*) _syscall(SC_getpargs);
}


errno_t Process_AllocateAddressSpace(size_t nbytes, void **ptr)
{
    return _syscall(SC_alloc_address_space, nbytes, ptr);
}


errno_t nanosleep(const struct timespec *delay)
{
    return (errno_t)_syscall(SC_sleep, delay);
}

errno_t usleep(useconds_t delay)
{
    const useconds_t ONE_SECOND = 1000 * 1000;

    if (delay >= ONE_SECOND) {
        return EINVAL;
    }
    if (delay <= 0) {
        return 0;
    }

    struct timespec ts;
    ts.tv_sec = delay / ONE_SECOND;
    ts.tv_nsec = delay - ts.tv_sec * ONE_SECOND;
    return nanosleep(&ts);
}

errno_t sleep(time_t delay)
{
    if (delay <= 0) {
        return 0;
    }

    struct timespec ts;
    ts.tv_sec = delay;
    ts.tv_nsec = 0;
    return nanosleep(&ts);
}
