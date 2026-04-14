//
//  Process.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/12/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "ProcessPriv.h"
#include <assert.h>
#include <string.h>
#include <filemanager/FileHierarchy.h>
#include <kern/kalloc.h>


static const char*      g_systemd_argv[2] = { "/System/Commands/systemd", NULL };
static const char*      g_kernel_argv[2] = { "kerneld", NULL };
static const char*      g_kernel_env[1] = { NULL };
static proc_args_t      g_kernel_pargs;
static struct Process   g_kernel_proc_storage;
ProcessRef _Nonnull gKernelProcess;



static void Process_Init(ProcessRef _Nonnull self, pid_t ppid, pid_t pgrp, pid_t sid, FileHierarchyRef _Nonnull fh, uid_t uid, gid_t gid, InodeRef _Nonnull pRootDir, InodeRef _Nonnull pWorkingDir, fs_perms_t umask)
{
    assert(ppid > 0);

    mtx_init(&self->mtx);
    AddressSpace_Init(&self->addr_space);

    self->retainCount = RC_INIT;
    self->run_state = PROC_STATE_RESUMED;
    self->pid = 0;
    self->ppid = ppid;
    self->pgrp = pgrp;
    self->sid = sid;

    self->vcpu_queue = DEQUE_INIT;
    self->next_avail_vcpuid = VCPUID_MAIN + 1;

    self->exit_reason = 0;
    
    IOChannelTable_Init(&self->ioChannelTable);

    for (size_t i = 0; i < UWQ_HASH_CHAIN_COUNT; i++) {
        self->waitQueueTable[i] = DEQUE_INIT;
    }
    self->nextAvailWaitQueueId = 0;

    clock_gettime(g_mono_clock, &self->creation_time);

    wq_init(&self->clk_wait_queue);
    wq_init(&self->siwa_queue);
    _proc_init_default_sigroutes(self);
    FileManager_Init(&self->fm, fh, uid, gid, pRootDir, pWorkingDir, umask);
}

errno_t Process_Create(pid_t ppid, pid_t pgrp, pid_t sid, FileHierarchyRef _Nonnull fh, uid_t uid, gid_t gid, InodeRef _Nonnull pRootDir, InodeRef _Nonnull pWorkingDir, fs_perms_t umask, ProcessRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    ProcessRef self;
    
    err = kalloc_cleared(sizeof(Process), (void**)&self);
    if (err == EOK) {
        Process_Init(self, ppid, pgrp, sid, fh, uid, gid, pRootDir, pWorkingDir, umask);
    }

    *pOutSelf = self;
    return err;
}

static void _proc_deinit(ProcessRef _Nonnull self)
{
    IOChannelTable_Deinit(&self->ioChannelTable);
    FileManager_Deinit(&self->fm);
    _proc_destroy_sigroutes(self);

    for (size_t i = 0; i < UWQ_HASH_CHAIN_COUNT; i++) {
        deque_for_each(&self->waitQueueTable[i], struct u_wait_queue, it,
            uwq_destroy(it);
        )
    }

    wq_deinit(&self->siwa_queue);
    wq_deinit(&self->clk_wait_queue);
    AddressSpace_Deinit(&self->addr_space);
    self->pargs_base = NULL;

    self->pid = 0;
    self->ppid = 0;

    mtx_deinit(&self->mtx);
}

ProcessRef _Nonnull Process_Retain(ProcessRef _Nonnull self)
{
    rc_retain(&self->retainCount);
    return self;
}

void Process_Release(ProcessRef _Nullable self)
{
    if (self && rc_release(&self->retainCount)) {
        _proc_deinit(self);
        kfree(self);
    }
}

int Process_GetState(ProcessRef _Nonnull self)
{
    mtx_lock(&self->mtx);
    const int state = self->run_state;
    mtx_unlock(&self->mtx);

    return state;
}

pid_t Process_GetId(ProcessRef _Nonnull self)
{
    return self->pid;
}

pid_t Process_GetParentId(ProcessRef _Nonnull self)
{
    return self->ppid;
}

pid_t Process_GetSessionId(ProcessRef _Nonnull self)
{
    return self->sid;
}

uid_t Process_GetUserId(ProcessRef _Nonnull self)
{
    return FileManager_GetRealUserId(&self->fm);
}

static void _vcpu_relinquish_self(void)
{
    vcpu_t vp = vcpu_current();

    Process_RelinquishVirtualProcessor(vp->proc, vp);
    /* NOT REACHED */
}

errno_t Process_AcquireVirtualProcessor(ProcessRef _Nonnull self, const _vcpu_acquire_attr_t* _Nonnull attr, vcpu_t _Nullable * _Nonnull pOutVp)
{
    decl_try_err();
    const bool is_uproc = (self != gKernelProcess) ? true : false;
    vcpu_t vp = NULL;
    vcpu_acquisition_t ac;

    mtx_lock(&self->mtx);
    if (vcpu_is_aborting(vcpu_current())) {
        throw(EINTR);
    }

    ac.func = (VoidFunc_1)attr->func;
    ac.arg = attr->arg;
    ac.ret_func = (is_uproc) ? vcpu_uret_relinquish_self : _vcpu_relinquish_self;
    ac.kernelStackBase = NULL;
    ac.kernelStackSize = 0;
    ac.userStackSize = (is_uproc) ? __max(attr->stack_size, PROC_DEFAULT_USER_STACK_SIZE) : 0;
    ac.id = self->next_avail_vcpuid++;
    ac.group_id = attr->group_id;
    ac.policy = attr->policy;
    ac.isUser = is_uproc;

    try(vcpu_acquire(&ac, &vp));
    vcpu_set_quantum_boost(vp, self->quantum_boost);

    vp->proc = self;
    vp->udata = attr->data;
    deque_add_last(&self->vcpu_queue, &vp->owner_qe);
    self->vcpu_count++;
    self->vcpu_lifetime_count++;
    
catch:
    mtx_unlock(&self->mtx);

    if (err == EOK) {
        *pOutVp = vp;

        if ((attr->flags & VCPU_ACQUIRE_RESUMED) == VCPU_ACQUIRE_RESUMED) {
            vcpu_resume(vp, false);
        }
    }

    return err;
}

void Process_RelinquishVirtualProcessor(ProcessRef _Nonnull self, vcpu_t _Nonnull vp)
{
    Process_DetachVirtualProcessor(self, vp);
    vcpu_relinquish(vcpu_current());
    /* NOT REACHED */
}

void Process_DetachVirtualProcessor(ProcessRef _Nonnull self, vcpu_t _Nonnull vp)
{
    assert(vp->proc == self);

    mtx_lock(&self->mtx);
    deque_remove(&self->vcpu_queue, &vp->owner_qe);

    self->rq_user_ticks += vp->user_ticks;
    self->rq_system_ticks += vp->system_ticks;
    self->rq_wait_ticks += vp->wait_ticks;
    self->vcpu_count--;

    mtx_unlock(&self->mtx);
}

errno_t Process_GetVirtualProcessorIds(ProcessRef _Nonnull self, vcpuid_t* _Nonnull buf, size_t bufSize, int* _Nonnull out_hasMore)
{
    decl_try_err();
    size_t idx = 0;

    // Min is 2 because there must be one vcpu that is executing this code + the trailing 0
    if (bufSize < 2) {
        return ERANGE;
    }

    mtx_lock(&self->mtx);
    deque_for_each(&self->vcpu_queue, deque_node_t, it,
        vcpu_t cvp = vcpu_from_owner_qe(it);

        buf[idx++] = cvp->id;
        if (idx == bufSize-1) {
            break;
        }
    )

    buf[idx] = 0;
    *out_hasMore = (self->vcpu_count > idx) ? 1 : 0;

    mtx_unlock(&self->mtx);

    return err;
}

void Process_GetSigcred(ProcessRef _Nonnull self, sigcred_t* _Nonnull cred)
{
    cred->pid = self->pid;
    cred->ppid = self->ppid;
    cred->uid = FileManager_GetRealUserId(&self->fm);
}

errno_t Process_GetSchedParam(ProcessRef _Nonnull self, int type, int* _Nonnull param)
{
    decl_try_err();

    mtx_lock(&self->mtx);
    switch (type) {
        case PROC_SCHED_QUANTUM_BOOST:
            *param = self->quantum_boost;
            break;

        case PROC_SCHED_NICE:
            *param = self->sched_nice;
            break;

        default:
            err = EINVAL;
            break;
    }
    mtx_unlock(&self->mtx);

    return err;
}

errno_t Process_SetSchedParam(ProcessRef _Nonnull self, int type, const int* _Nonnull param)
{
    decl_try_err();

    mtx_lock(&self->mtx);
    switch (type) {
        case PROC_SCHED_QUANTUM_BOOST:
            deque_for_each(&self->vcpu_queue, vcpu_t, it,
                vcpu_t cvp = vcpu_from_owner_qe(it);
                const int sps = preempt_disable();

                vcpu_set_quantum_boost(cvp, *param);
                preempt_restore(sps);
            );
            break;

        case PROC_SCHED_NICE:
            deque_for_each(&self->vcpu_queue, vcpu_t, it,
                vcpu_t cvp = vcpu_from_owner_qe(it);
                const int sps = preempt_disable();

                vcpu_set_nice(cvp, *param);
                preempt_restore(sps);
            );
            break;

        default:
            err = EINVAL;
            break;
    }
    mtx_unlock(&self->mtx);

    return err;
}

// 'bufSize' has to be >= 1
static errno_t _proc_path(ProcessRef _Nonnull self, char* _Nonnull buf, size_t bufSize)
{
    decl_try_err();
    const proc_args_t* pa = (const proc_args_t*)self->pargs_base;
    const char* arg0 = (pa) ? pa->argv[0] : NULL;
    const size_t arg0len = (arg0) ? strlen(arg0) : 0;

    if (bufSize >= arg0len + 1) {
        memcpy(buf, arg0, arg0len);
        buf[arg0len] = '\0';
    }
    else {
        *buf = '\0';
        return ERANGE;
    }

    return err;
}

// 'bufSize' has to be >= 1
static errno_t _proc_name(ProcessRef _Nonnull self, char* _Nonnull buf, size_t bufSize)
{
    decl_try_err();
    const proc_args_t* pa = (const proc_args_t*)self->pargs_base;
    const char* arg0 = (pa) ? pa->argv[0] : NULL;
    const char* fname = (arg0) ? strrchr(arg0, '/') + 1 : arg0;
    const size_t fnameLen = (fname) ? strlen(fname) : 0;

    if (bufSize >= fnameLen + 1) {
        memcpy(buf, fname, fnameLen);
        buf[fnameLen] = '\0';
    }
    else {
        *buf = '\0';
        return ERANGE;
    }

    return err;
}

errno_t Process_GetProperty(ProcessRef _Nonnull self, int flavor, char* _Nonnull buf, size_t bufSize)
{
    decl_try_err();

    if (bufSize < 1) {
        return ERANGE;
    }

    mtx_lock(&self->mtx);

    switch (flavor) {
        case PROC_PROP_CWD:
            err = FileManager_GetWorkingDirectoryPath(&self->fm, buf, bufSize);
            break;

        case PROC_PROP_PATH:
            err = _proc_path(self, buf, bufSize);
            break;

        case PROC_PROP_NAME:
            err = _proc_name(self, buf, bufSize);
            break;

        case PROC_PROP_ARGS: {
            const size_t argv_size = ((proc_args_t*)self->pargs_base)->argv_size;

            if (bufSize >= argv_size) {
                memcpy(buf, ((proc_args_t*)self->pargs_base)->argv, argv_size);
            }
            else {
                *buf = '\0';
                err = ERANGE;
            }
            break;
        }

        case PROC_PROP_ENVIRON: {
            const size_t env_size = ((proc_args_t*)self->pargs_base)->env_size;

            if (bufSize >= env_size) {
                memcpy(buf, ((proc_args_t*)self->pargs_base)->envp, env_size);
            }
            else {
                *buf = '\0';
                err = ERANGE;
            }
            break;
        }

        default:
            err = EINVAL;
            break;
    }

    mtx_unlock(&self->mtx);

    return err;
}

// Returns a copy of the receiver's information.
errno_t Process_GetInfo(ProcessRef _Nonnull self, int flavor, proc_info_ref _Nonnull info)
{
    decl_try_err();

    mtx_lock(&self->mtx);

    switch (flavor) {
        case PROC_INFO_BASIC: {
            proc_basic_info_t* ip = info;

            ip->run_state = self->run_state;
            ip->vcpu_count = self->vcpu_count;
            ip->vcpu_lifetime_count = self->vcpu_lifetime_count;
            ip->vcpu_waiting_count = self->vcpu_waiting_count;
            ip->vm_size = AddressSpace_GetVirtualSize(&self->addr_space);
            ip->argv_size = ((proc_args_t*)self->pargs_base)->argv_size;
            ip->env_size = ((proc_args_t*)self->pargs_base)->env_size;
            break;
        }

        case PROC_INFO_IDS: {
            proc_ids_info_t* ip = info;

            ip->id = self->pid;
            ip->parent_id = self->ppid;
            ip->group_id = self->pgrp;
            ip->session_id = self->sid;
            break;
        }

        case PROC_INFO_USER: {
            proc_user_info_t* ip = info;

            ip->uid = FileManager_GetRealUserId(&self->fm);
            ip->gid = FileManager_GetRealGroupId(&self->fm);
            ip->umask = FileManager_GetUMask(&self->fm);
            break;
        }

        case PROC_INFO_TIMES: {
            proc_times_info_t* ip = info;
            ticks_t tt;

            ip->creation_time = self->creation_time;
            clock_ticks2time(g_mono_clock, self->user_ticks, &ip->user_time);
            clock_ticks2time(g_mono_clock, self->system_ticks, &ip->system_time);
            clock_ticks2time(g_mono_clock, self->wait_ticks, &ip->wait_time);

            tt = self->user_ticks - self->rq_user_ticks;
            clock_ticks2time(g_mono_clock, tt, &ip->user_time);
            tt = self->system_ticks - self->rq_system_ticks;
            clock_ticks2time(g_mono_clock, tt, &ip->system_time);
            tt = self->wait_ticks - self->rq_wait_ticks;
            clock_ticks2time(g_mono_clock, tt, &ip->wait_time);
            break;
        }
        
        default:
            err = EINVAL;
            break;
    }

    mtx_unlock(&self->mtx);

    return err;
}

void _proc_terminate(ProcessRef _Nonnull _Locked self, int signo)
{
    _proc_set_exit_reason(self, PROC_STATUS_SIGNALED, signo)
    vcpu_send_signal(vcpu_from_owner_qe(self->vcpu_queue.first), SIG_TERMINATE);
}

void Process_Terminate(ProcessRef _Nonnull self)
{
    mtx_lock(&self->mtx);
    _proc_terminate(self, SIG_TERMINATE);
    mtx_unlock(&self->mtx);
}

// Suspend all vcpus in the process if the process is currently in running state.
// Otherwise does nothing. Nesting is not supported.
void _proc_suspend(ProcessRef _Nonnull _Locked self)
{
    if (self->run_state == PROC_STATE_RESUMED) {
        deque_for_each(&self->vcpu_queue, deque_node_t, it,
            vcpu_t cvp = vcpu_from_owner_qe(it);

            vcpu_suspend(cvp);
        )
        self->run_state = PROC_STATE_SUSPENDED;
    }
}

void Process_Suspend(ProcessRef _Nonnull self)
{
    mtx_lock(&self->mtx);
    _proc_suspend(self);
    mtx_unlock(&self->mtx);
}

// Resume all vcpus in the process if the process is currently in stopped state.
// Otherwise does nothing.
void _proc_resume(ProcessRef _Nonnull _Locked self)
{
    if (self->run_state == PROC_STATE_SUSPENDED) {
        deque_for_each(&self->vcpu_queue, deque_node_t, it,
            vcpu_t cvp = vcpu_from_owner_qe(it);

            vcpu_resume(cvp, false);
        )
        self->run_state = PROC_STATE_RESUMED;
    }
}

void Process_Resume(ProcessRef _Nonnull self)
{
    mtx_lock(&self->mtx);
    _proc_resume(self);
    mtx_unlock(&self->mtx);
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Kernel Process

void KernelProcess_Init(FileHierarchyRef _Nonnull pRootFh, ProcessRef _Nullable * _Nonnull pOutSelf)
{
    InodeRef rootDir = FileHierarchy_AcquireRootDirectory(pRootFh);
    
    Process_Init(&g_kernel_proc_storage, PID_KERNEL, 0, 0, pRootFh, UID_ROOT, GID_ROOT, rootDir, rootDir, perm_from_octal(0022));
    Inode_Relinquish(rootDir);

    g_kernel_pargs.version = sizeof(proc_args_t);
    g_kernel_pargs.argc = 1;
    g_kernel_pargs.argv = g_kernel_argv;
    g_kernel_pargs.envp = g_kernel_env;
    g_kernel_proc_storage.pargs_base = (char*)&g_kernel_pargs;

    vcpu_t main_vp = vcpu_current();
    main_vp->proc = &g_kernel_proc_storage;
    main_vp->id = VCPUID_MAIN;
    main_vp->group_id = VCPUID_MAIN_GROUP;
    deque_add_last(&g_kernel_proc_storage.vcpu_queue, &main_vp->owner_qe);
    g_kernel_proc_storage.vcpu_count++;

    *pOutSelf = &g_kernel_proc_storage;
}

errno_t KernelProcess_SpawnSystemd(ProcessRef _Nonnull self, FileHierarchyRef _Nonnull fh)
{
    proc_spawn_t opts = (proc_spawn_t){0};

    opts.options = PROC_SPAWN_GROUP_LEADER | PROC_SPAWN_SESSION_LEADER;

    return Process_SpawnChild(self, g_systemd_argv[0], g_systemd_argv, &opts, fh, NULL);
}