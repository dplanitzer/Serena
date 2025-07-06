//
//  proc.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/5/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "syscalldecls.h"
#include <dispatcher/delay.h>


// XXX Will be removed when we'll do the process termination algorithm
static WaitQueue gHackQueue;

SYSCALL_1(exit, int status)
{    
    // Trigger the termination of the process. Note that the actual termination
    // is done asynchronously. That's why we sleep below since we don't want to
    // return to user space anymore.
    Process_Terminate((ProcessRef)p, pa->status);


    // This wait here will eventually be aborted when the dispatch queue that
    // owns this VP is terminated. This interrupt will be caused by the abort
    // of the call-as-user and thus this system call will not return to user
    // space anymore. Instead it will return to the dispatch queue main loop.
    _sleep(&gHackQueue, NULL, WAIT_ABSTIME, &TIMESPEC_INF, NULL);
    return 0;
}

SYSCALL_4(spawn_process, const char* _Nonnull path, const char* _Nullable * _Nullable argv, const spawn_opts_t* _Nonnull options, pid_t* _Nullable pOutPid)
{
    return Process_SpawnChildProcess((ProcessRef)p, pa->path, pa->argv, pa->options, pa->pOutPid);
}

SYSCALL_0(getpid)
{
    return ((ProcessRef)p)->pid;
}

SYSCALL_0(getppid)
{
    return ((ProcessRef)p)->ppid;
}

SYSCALL_0(getuid)
{
    return FileManager_GetRealUserId(&((ProcessRef)p)->fm);
}

SYSCALL_0(getgid)
{
    return FileManager_GetRealGroupId(&((ProcessRef)p)->fm);
}

SYSCALL_0(getpargs)
{
    return (intptr_t) ((ProcessRef)p)->argumentsBase;
}

SYSCALL_3(waitpid, pid_t pid, struct _pstatus* _Nonnull pOutStatus, int options)
{
    return Process_WaitForTerminationOfChild((ProcessRef)p, pa->pid, pa->pOutStatus, pa->options);
}
