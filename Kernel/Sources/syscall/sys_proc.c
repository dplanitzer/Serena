//
//  sys_proc.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/5/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "syscalldecls.h"
#include <kpi/wait.h>


SYSCALL_1(exit, int status)
{    
    Process_Exit(vp->proc, WMAKEEXITED(pa->status));
    /* NOT REACHED */
    return 0;
}

SYSCALL_4(spawn_process, const char* _Nonnull path, const char* _Nullable * _Nullable argv, const spawn_opts_t* _Nonnull options, pid_t* _Nullable pOutPid)
{
    return Process_SpawnChildProcess(vp->proc, pa->path, pa->argv, pa->options, pa->pOutPid);
}

SYSCALL_0(getpid)
{
    return vp->proc->pid;
}

SYSCALL_0(getppid)
{
    return vp->proc->ppid;
}

SYSCALL_0(getpgrp)
{
    return vp->proc->pgrp;
}

SYSCALL_0(getsid)
{
    return vp->proc->sid;
}

SYSCALL_0(getuid)
{
    return FileManager_GetRealUserId(&vp->proc->fm);
}

SYSCALL_0(getgid)
{
    return FileManager_GetRealGroupId(&vp->proc->fm);
}

SYSCALL_0(getpargs)
{
    return (intptr_t) vp->proc->argumentsBase;
}

SYSCALL_3(waitpid, pid_t pid, struct _pstatus* _Nonnull pOutStatus, int options)
{
    return Process_WaitForTerminationOfChild(vp->proc, pa->pid, pa->pOutStatus, pa->options);
}
