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


SYSCALL_0(proc_self)
{
    return Process_GetId(vp->proc);
}

SYSCALL_1(proc_exit, int status)
{
    Process_Exit(vp->proc, PROC_STATUS_EXITED, pa->status);
    /* NOT REACHED */
    return 0;
}

SYSCALL_5(proc_spawn, int wd, const char* _Nonnull path, const char* _Nullable * _Nullable argv, const proc_spawn_t* _Nonnull options, pid_t* _Nullable pOutPid)
{
    if (pa->wd != FD_CWD) {
        //XXX not yet
        return EINVAL;
    }

    return Process_SpawnChild(vp->proc, pa->path, pa->argv, pa->options, NULL, pa->pOutPid);
}

SYSCALL_4(proc_exec, int wd, const char* _Nonnull path, const char* _Nullable * _Nullable argv, const char* _Nullable * _Nullable envp)
{
    decl_try_err();

    if (pa->wd != FD_CWD) {
        //XXX not yet
        return EINVAL;
    }

    err = Process_Exec(vp->proc, pa->path, pa->argv, pa->envp, true);
    if (err == EOK) {
        vcpu_relinquish(vp);
        /* NOT REACHED */
    }

    return err;
}

SYSCALL_4(proc_status, int of, pid_t id, int flags, proc_status_t* _Nonnull ps)
{
    return Process_GetStatus(vp->proc, pa->of, pa->id, pa->flags, pa->ps);
}

SYSCALL_1(proc_terminate, pid_t pid)
{
    decl_try_err();
    ProcessRef pp = vp->proc;

    if (pa->pid == PROC_SELF || pa->pid == pp->pid) {
        Process_Terminate(pp);
    }
    else {
        ProcessRef the_pp = ProcessManager_CopyProcessForPid(gProcessManager, pa->pid);

        if (the_pp) {
            Process_Terminate(the_pp);
            Process_Release(the_pp);
        }
        else {
            err = ESRCH;
        }
    }

    return err;
}

SYSCALL_1(proc_suspend, pid_t pid)
{
    decl_try_err();
    ProcessRef pp = vp->proc;

    if (pa->pid == PROC_SELF || pa->pid == pp->pid) {
        Process_Suspend(pp);
    }
    else {
        ProcessRef the_pp = ProcessManager_CopyProcessForPid(gProcessManager, pa->pid);

        if (the_pp) {
            Process_Suspend(the_pp);
            Process_Release(the_pp);
        }
        else {
            err = ESRCH;
        }
    }

    return err;
}

SYSCALL_1(proc_resume, pid_t pid)
{
    decl_try_err();
    ProcessRef pp = vp->proc;

    if (pa->pid == PROC_SELF || pa->pid == pp->pid) {
        Process_Resume(pp);
    }
    else {
        ProcessRef the_pp = ProcessManager_CopyProcessForPid(gProcessManager, pa->pid);

        if (the_pp) {
            Process_Resume(the_pp);
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

SYSCALL_4(proc_property, pid_t pid, int flavor, char* _Nonnull buf, size_t bufSize)
{
    decl_try_err();
    ProcessRef pp = vp->proc;

    if (pa->pid == PROC_SELF || pa->pid == pp->pid) {
        err = Process_GetProperty(pp, pa->flavor, pa->buf, pa->bufSize);
    }
    else {
        ProcessRef the_pp = ProcessManager_CopyProcessForPid(gProcessManager, pa->pid);

        if (the_pp) {
            Process_GetProperty(the_pp, pa->flavor, pa->buf, pa->bufSize);
            Process_Release(the_pp);
        }
        else {
            err = ESRCH;
        }
    }

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

SYSCALL_1(proc_setumask, fs_perms_t umask)
{
    ProcessRef pp = vp->proc;

    mtx_lock(&pp->mtx);
    FileManager_SetUMask(&pp->fm, pa->umask);
    mtx_unlock(&pp->mtx);
    
    return EOK;
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

SYSCALL_3(proc_vcpus, vcpuid_t* _Nonnull buf, size_t bufSize, int* _Nonnull out_hasMore)
{
    return Process_GetVirtualProcessorIds(vp->proc, pa->buf, pa->bufSize, pa->out_hasMore);
}
