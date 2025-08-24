//
//  sys_proc.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/5/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "syscalldecls.h"
#include <kpi/wait.h>
#include <sched/vcpu_pool.h>


SYSCALL_1(exit, int status)
{    
    Process_Exit(vp->proc, JREASON_EXIT, pa->status);
    /* NOT REACHED */
    return 0;
}

SYSCALL_4(spawn_process, const char* _Nonnull path, const char* _Nullable * _Nullable argv, const spawn_opts_t* _Nonnull options, pid_t* _Nullable pOutPid)
{
    return Process_SpawnChild(vp->proc, pa->path, pa->argv, pa->options, NULL, pa->pOutPid);
}

SYSCALL_3(proc_exec, const char* _Nonnull path, const char* _Nullable * _Nullable argv, const char* _Nullable * _Nullable envp)
{
    const errno_t err = Process_Exec(vp->proc, pa->path, pa->argv, pa->envp, true);

    if (err == EOK) {
        vcpu_pool_relinquish(g_vcpu_pool, vp);
        /* NOT REACHED */
    }

    return err;
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
    return (intptr_t) vp->proc->pargs_base;
}

SYSCALL_5(proc_timedjoin, int scope, pid_t id, int flags, const struct timespec* _Nonnull wtp, struct proc_status* _Nonnull ps)
{
    return Process_TimedJoin(vp->proc, pa->scope, pa->id, pa->flags, pa->wtp, pa->ps);
}
