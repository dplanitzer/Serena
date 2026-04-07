//
//  sys_proc.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/5/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "syscalldecls.h"
#include <kpi/process.h>
#include <process/ProcessManager.h>


SYSCALL_0(getpargs)
{
    return (intptr_t) vp->proc->pargs_base;
}

SYSCALL_1(proc_exit, int status)
{    
    Process_Exit(vp->proc, JREASON_EXIT, pa->status);
    /* NOT REACHED */
    return 0;
}

SYSCALL_3(proc_exec, const char* _Nonnull path, const char* _Nullable * _Nullable argv, const char* _Nullable * _Nullable envp)
{
    const errno_t err = Process_Exec(vp->proc, pa->path, pa->argv, pa->envp, true);

    if (err == EOK) {
        vcpu_relinquish(vp);
        /* NOT REACHED */
    }

    return err;
}

SYSCALL_4(spawn_process, const char* _Nonnull path, const char* _Nullable * _Nullable argv, const spawn_opts_t* _Nonnull options, pid_t* _Nullable pOutPid)
{
    return Process_SpawnChild(vp->proc, pa->path, pa->argv, pa->options, NULL, pa->pOutPid);
}

SYSCALL_5(proc_timedjoin, int scope, pid_t id, int flags, const struct timespec* _Nonnull wtp, struct proc_status* _Nonnull ps)
{
    return Process_TimedJoin(vp->proc, pa->scope, pa->id, pa->flags, pa->wtp, pa->ps);
}

SYSCALL_2(proc_cwd, char* _Nonnull buffer, size_t bufferSize)
{
    ProcessRef pp = vp->proc;

    mtx_lock(&pp->mtx);
    const errno_t err = FileManager_GetWorkingDirectoryPath(&pp->fm, pa->buffer, pa->bufferSize);
    mtx_unlock(&pp->mtx);
    return err;
}

SYSCALL_1(proc_setcwd, const char* _Nonnull path)
{
    ProcessRef pp = vp->proc;

    mtx_lock(&pp->mtx);
    const errno_t err = FileManager_SetWorkingDirectoryPath(&pp->fm, pa->path);
    mtx_unlock(&pp->mtx);
    return err;
}

SYSCALL_1(proc_umask, mode_t mask)
{
    ProcessRef pp = vp->proc;
    mode_t omask;

    mtx_lock(&pp->mtx);
    if (pa->mask != SEO_UMASK_NO_CHANGE) {
        omask = FileManager_UMask(&pp->fm, pa->mask);
    }
    else {
        omask = FileManager_GetUMask(&pp->fm);
    }
    mtx_unlock(&pp->mtx);
    
    return (intptr_t)omask;
}

SYSCALL_3(proc_schedparam, pid_t pid, int type, int* _Nonnull param)
{
    decl_try_err();
    ProcessRef pp = vp->proc;

    if (pa->pid == PROC_SELF || pa->pid == pp->pid) {
        err = Process_GetSchedParam(pp, pa->type, pa->param);
    }
    else {
        ProcessRef the_pp = ProcessManager_CopyProcessForPid(gProcessManager, pa->pid);

        if (the_pp) {
            Process_GetSchedParam(the_pp, pa->type, pa->param);
            Process_Release(the_pp);
        }
        else {
            err = ESRCH;
        }
    }

    return err;
}

SYSCALL_3(proc_setschedparam, pid_t pid, int type, int* _Nonnull param)
{
    decl_try_err();
    ProcessRef pp = vp->proc;

    if (pa->pid == PROC_SELF || pa->pid == pp->pid) {
        err = Process_SetSchedParam(pp, pa->type, pa->param);
    }
    else {
        ProcessRef the_pp = ProcessManager_CopyProcessForPid(gProcessManager, pa->pid);

        if (the_pp) {
            Process_SetSchedParam(the_pp, pa->type, pa->param);
            Process_Release(the_pp);
        }
        else {
            err = ESRCH;
        }

    }

    return err;
}

SYSCALL_3(proc_info, pid_t pid, int flavor, proc_info_ref _Nonnull info)
{
    decl_try_err();
    ProcessRef pp = vp->proc;

    if (pa->pid == PROC_SELF || pa->pid == pp->pid) {
        err = Process_GetInfo(pp, pa->flavor, pa->info);
    }
    else {
        ProcessRef the_pp = ProcessManager_CopyProcessForPid(gProcessManager, pa->pid);

        if (the_pp) {
            Process_GetInfo(the_pp, pa->flavor, pa->info);
            Process_Release(the_pp);
        }
        else {
            err = ESRCH;
        }
    }

    return err;
}

SYSCALL_3(proc_name, pid_t pid, char* _Nonnull buf, size_t bufSize)
{
    decl_try_err();
    ProcessRef pp = vp->proc;

    if (pa->pid == PROC_SELF || pa->pid == pp->pid) {
        err = Process_GetName(pp, pa->buf, pa->bufSize);
    }
    else {
        ProcessRef the_pp = ProcessManager_CopyProcessForPid(gProcessManager, pa->pid);

        if (the_pp) {
            Process_GetName(the_pp, pa->buf, pa->bufSize);
            Process_Release(the_pp);
        }
        else {
            err = ESRCH;
        }

    }

    return err;
}

SYSCALL_3(proc_vcpus, vcpuid_t* _Nonnull buf, size_t bufSize, int* _Nonnull out_hasMore)
{
    return Process_GetVirtualProcessorIds(vp->proc, pa->buf, pa->bufSize, pa->out_hasMore);
}
