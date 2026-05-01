//
//  Process.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/12/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "ProcessPriv.h"
#include "kerneld.h"
#include <assert.h>
#include <string.h>
#include <filemanager/FileHierarchy.h>
#include <kern/kalloc.h>


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
    self->ctx_base = NULL;

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
    ac.sched_nice = self->sched_nice;
    ac.sched_quantum_boost = self->quantum_boost;
    ac.isUser = is_uproc;

    try(vcpu_acquire(&ac, &vp));
    
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

errno_t Process_SetExceptionHandler(ProcessRef _Nonnull self, const excpt_handler_t* _Nullable handler, excpt_handler_t* _Nullable old_handler)
{
    if (handler->func == NULL) {
        return EINVAL;
    }

    //XXX not great. Use suitable atomic functions instead
    const int sps = preempt_disable();
    if (old_handler) {
        *old_handler = self->excpt_handler;
    }
    self->excpt_handler = *handler;
    preempt_restore(sps);

    return EOK;
}

void Process_GetExceptionHandler(ProcessRef _Nonnull self, excpt_handler_t* _Nonnull handler)
{
    const int sps = preempt_disable();
    *handler = self->excpt_handler;
    preempt_restore(sps);
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
        case SCHED_QUANTUM_BOOST:
            *param = self->quantum_boost;
            break;

        case SCHED_NICE:
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
    bool hasChanged = false;

    mtx_lock(&self->mtx);
    switch (type) {
        case SCHED_QUANTUM_BOOST: {
            const int new_boost = VCPU_CLAMPED_QUANTUM_BOOST(*param);
            hasChanged = new_boost != self->quantum_boost;
            self->quantum_boost = new_boost;
            break;
        }

        case SCHED_NICE: {
            const int new_nice = VCPU_CLAMPED_NICE_PRIORITY(*param);
            hasChanged = new_nice != self->sched_nice;
            self->sched_nice = new_nice;
            break;
        }

        default:
            err = EINVAL;
            break;
    }

    if (err == EOK && hasChanged) {
        deque_for_each(&self->vcpu_queue, vcpu_t, it,
            vcpu_t cvp = vcpu_from_owner_qe(it);
            const int sps = preempt_disable();

            switch (type) {
                case SCHED_QUANTUM_BOOST:
                    vcpu_set_quantum_boost(cvp, self->quantum_boost);
                    break;

                case SCHED_NICE:
                    vcpu_set_nice(cvp, self->sched_nice);
                    break;
            }

            vcpu_on_sched_param_changed(cvp);
            preempt_restore(sps);
        );
    }
    mtx_unlock(&self->mtx);

    return err;
}

// 'bufSize' has to be >= 1
static errno_t _proc_path(ProcessRef _Nonnull self, char* _Nonnull buf, size_t bufSize)
{
    decl_try_err();
    const char* arg0 = (self->ctx_base) ? self->ctx_base->argv[0] : NULL;
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
    const char* arg0 = (self->ctx_base) ? self->ctx_base->argv[0] : NULL;
    const char* lpc = (arg0) ? strrchr(arg0, '/') : NULL;
    const char* fname = (lpc) ? lpc + 1 : arg0;
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

        case PROC_PROP_CMDLINE:
            if (bufSize >= self->arg_size) {
                memcpy(buf, self->arg_strings, self->arg_size);
            }
            else {
                *buf = '\0';
                err = ERANGE;
            }
            break;

        case PROC_PROP_ENVIRON:
            if (bufSize >= self->env_size) {
                memcpy(buf, self->env_strings, self->env_size);
            }
            else {
                *buf = '\0';
                err = ERANGE;
            }
            break;

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

            ip->pid = self->pid;
            ip->ppid = self->ppid;
            ip->pgrp = self->pgrp;
            ip->sid = self->sid;

            ip->uid = FileManager_GetRealUserId(&self->fm);
            ip->gid = FileManager_GetRealGroupId(&self->fm);
            ip->umask = FileManager_GetUMask(&self->fm);
            
            ip->run_state = self->run_state;
            ip->flags = 0;
            
            ip->vcpu_count = self->vcpu_count;
            ip->vcpu_lifetime_count = self->vcpu_lifetime_count;
            ip->vcpu_waiting_count = self->vcpu_waiting_count;
            ip->vm_size = AddressSpace_GetVirtualSize(&self->addr_space);
            ip->cmdline_size = self->arg_size;
            ip->env_size = self->env_size;
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

// Suspend all vcpus in the process if the process is currently in running state.
// Otherwise does nothing. Nesting is not supported.
void _proc_suspend(ProcessRef _Nonnull _Locked self)
{
    if (self->run_state == PROC_STATE_RESUMED) {
        deque_for_each(&self->vcpu_queue, deque_node_t, it,
            vcpu_t cvp = vcpu_from_owner_qe(it);

            vcpu_suspend(cvp);
        )
        _proc_set_state(self, PROC_STATE_SUSPENDED, false);
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
        _proc_set_state(self, PROC_STATE_RESUMED, false);
    }
}

void Process_Resume(ProcessRef _Nonnull self)
{
    mtx_lock(&self->mtx);
    _proc_resume(self);
    mtx_unlock(&self->mtx);
}
