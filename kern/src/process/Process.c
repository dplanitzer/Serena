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
    HandlerTable_Deinit(&self->HandlerTable);
    FileManager_Deinit(&self->fm);
    _proc_destroy_sigroutes(self);
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

errno_t Process_GetPathForDriver(ProcessRef _Nonnull self, DriverRef _Nonnull driver, char* _Nonnull buf, size_t bufSize)
{
    decl_try_err();

    mtx_lock(&self->mtx);
    err = FileManager_GetPathForDriver(&self->fm, driver, buf, bufSize);
    mtx_unlock(&self->mtx);

    return err;
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
                    vcpu_set_quantum_boost_np(cvp, self->quantum_boost);
                    break;

                case SCHED_NICE:
                    vcpu_set_nice_np(cvp, self->sched_nice);
                    break;
            }

            vcpu_on_sched_param_changed_np(cvp);
            preempt_restore(sps);
        );
    }
    mtx_unlock(&self->mtx);

    return err;
}

// 'bufSize' has to be >= 1
static errno_t _proc_path(ProcessRef _Nonnull self, char* _Nonnull buf, size_t bufSize)
{
    const char* arg0 = (self->ctx_base) ? self->ctx_base->argv[0] : NULL;

    return strtobuf(buf, bufSize, (arg0) ? arg0 : "");
}

// 'bufSize' has to be >= 1
static errno_t _proc_name(ProcessRef _Nonnull self, char* _Nonnull buf, size_t bufSize)
{
    const char* arg0 = (self->ctx_base) ? self->ctx_base->argv[0] : NULL;
    const char* lpc = (arg0) ? strrchr(arg0, '/') : NULL;
    const char* fname = (lpc) ? lpc + 1 : arg0;

    return strtobuf(buf, bufSize, (fname) ? fname : "");
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
            clock_ticks2time(g_mono_clock, self->usr_ticks, &ip->user_time);
            clock_ticks2time(g_mono_clock, self->sys_ticks, &ip->system_time);
            clock_ticks2time(g_mono_clock, self->wait_ticks, &ip->wait_time);

            tt = self->usr_ticks - self->rq_usr_ticks;
            clock_ticks2time(g_mono_clock, tt, &ip->user_time);
            tt = self->sys_ticks - self->rq_sys_ticks;
            clock_ticks2time(g_mono_clock, tt, &ip->system_time);
            tt = self->wait_ticks - self->rq_wait_ticks;
            clock_ticks2time(g_mono_clock, tt, &ip->wait_time);
            break;
        }

        case PROC_INFO_IDS: {
            proc_ids_info_t* ip = info;

            ip->pid = self->pid;
            ip->ppid = self->ppid;
            ip->pgrp = self->pgrp;
            ip->sid = self->sid;
            break;
        }

        case PROC_INFO_USER: {
            proc_user_info_t* ip = info;

            ip->uid = FileManager_GetRealUserId(&self->fm);
            ip->gid = FileManager_GetRealGroupId(&self->fm);
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
void _proc_stop(ProcessRef _Nonnull _Locked self, int reason, int arg, bool notify_parent)
{
    vcpu_t this_vp = vcpu_current();

    if (self->run_state != PROC_STATE_RUNNING) {
        return;
    }

    _proc_set_state(self, PROC_STATE_STOPPED, reason, arg, notify_parent);


    // There are two possible ways that suspension may be done:
    // *) A vcpu that is NOT part of the process triggers suspension:
    //      -- all vcpus are suspended in a single atomic block
    //      -- no suspension coordinator is recorded
    //      -- we guarantee that all vcpus are suspended once we drop the proc lock
    // *) A vcpu that belongs to the process triggers suspension:
    //      -- all vcpus except the triggering one are suspended in a single atomic block
    //      -- the triggering vcpu is recorded as the suspension coordinator inside the atomic block
    //      -- the triggering vcpu is suspended after dropping the proc lock
    if (this_vp->proc != self) {
        // we can do the suspend in a single step if the vcpu that is triggering
        // the suspend is not part of the process 'self'
        this_vp = NULL;
    }
    self->stopper_vcpu = this_vp;


    // suspend all vcpus except the one that is executing this function
    deque_for_each(&self->vcpu_queue, deque_node_t, it,
        vcpu_t cvp = vcpu_from_owner_qe(it);

        if (cvp != this_vp) {
            vcpu_suspend(cvp);
        }
    )


    if (this_vp) {
        // the vcpu that is triggering the suspend is a member of the process
        // 'self'. We've suspended all other vcpus above and we're now suspending
        // this vcpu. Drop the lock, suspend it and reacquire the lock once the
        // suspend returns since the caller expects the mutex locked
        mtx_unlock(&self->mtx);
        vcpu_suspend(this_vp);
        mtx_lock(&self->mtx);
    }

    self->stopper_vcpu = NULL;
}

// Resume all vcpus in the process if the process is currently in stopped state.
// Otherwise does nothing.
void _proc_continue(ProcessRef _Nonnull _Locked self, int reason, int arg, bool notify_parent)
{
    if (self->run_state != PROC_STATE_STOPPED) {
        return;
    }

    // see the explanation in _proc_stop(): wait until the suspension coordinator
    // is suspended before proceeding with the resume. This is not necessary if
    // no suspension coordinator has been recorded.
    if (self->stopper_vcpu) {
        vcpu_await_suspension(self->stopper_vcpu);
    }


    _proc_set_state(self, PROC_STATE_RUNNING, reason, arg, notify_parent);


    deque_for_each(&self->vcpu_queue, deque_node_t, it,
        vcpu_t cvp = vcpu_from_owner_qe(it);

        vcpu_resume(cvp, false);
    )
}

void Process_Stop(ProcessRef _Nonnull self, int reason, int arg, bool notify_parent)
{
    mtx_lock(&self->mtx);
    _proc_stop(self, reason, arg, notify_parent);
    mtx_unlock(&self->mtx);
}

void Process_Continue(ProcessRef _Nonnull self, int reason, int arg, bool notify_parent)
{
    mtx_lock(&self->mtx);
    _proc_continue(self, reason, arg, notify_parent);
    mtx_unlock(&self->mtx);
}
