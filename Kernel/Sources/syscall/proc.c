//
//  proc.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/5/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "syscalldecls.h"


SYSCALL_1(exit, int status)
{    
    Process_Terminate((ProcessRef)p, pa->status);
    /* NOT REACHED */
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

SYSCALL_0(getpgrp)
{
    return ((ProcessRef)p)->pgrp;
}

SYSCALL_0(getsid)
{
    return ((ProcessRef)p)->sid;
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
