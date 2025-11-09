//
//  vcpu.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/21/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "vcpu.h"
#include "sched.h"
#include "vcpu_pool.h"
#include <machine/clock.h>
#include <machine/sched.h>
#include <kern/assert.h>
#include <kern/kalloc.h>
#include <kern/limits.h>
#include <kern/string.h>
#include <kern/timespec.h>


// Initializes a virtual processor. A virtual processor always starts execution
// in supervisor mode. The user stack size may be 0. Note that a virtual processor
// always starts out in suspended state.
//
// \param pVP the boot virtual processor record
// \param priority the initial VP priority
void vcpu_init(vcpu_t _Nonnull self, const sched_params_t* _Nonnull sched_params)
{
    assert(sched_params->type == SCHED_PARAM_QOS);

    memset(self, 0, sizeof(struct vcpu));
    stk_init(&self->kernel_stack);
    stk_init(&self->user_stack);
    
    self->timeout = CLOCK_DEADLINE_INIT;    
    self->sched_state = SCHED_STATE_INITIATED;

    self->flags = (g_sys_desc->fpu_model > FPU_MODEL_NONE) ? VP_FLAG_HAS_FPU : 0;
    self->qos = sched_params->u.qos.category;
    self->qos_priority = sched_params->u.qos.priority;

    self->dispatchQueueConcurrencyLaneIndex = -1;

    vcpu_sched_params_changed(self);
}

errno_t vcpu_acquire(const vcpu_acquisition_t* _Nonnull ac, vcpu_t _Nonnull * _Nonnull pOutVP)
{
    decl_try_err();
    bool doFree = false;


    // Try to get a vcpu from the global pool
    vcpu_t vp = vcpu_pool_checkout(g_vcpu_pool);


    // Create a new vcpu if we were not able to reuse a cached one
    if (vp == NULL) {
        try(kalloc_cleared(sizeof(struct vcpu), (void**) &vp));
        doFree = true;

        vcpu_init(vp, &ac->schedParams);
        vcpu_suspend(vp);
    }


    // Note that a vcpu freshly checked out from the pool may not have entered
    // the suspend state yet. Wait until it is actually suspended and before we
    // proceed with reconfiguring it. We only become the owner of the vcpu once
    // it has entered suspended state.
    while (!vcpu_suspended(vp)) {
        vcpu_yield();
    }

    
    // Configure the vcpu
    try(vcpu_setcontext(vp, ac, true));
    vcpu_setschedparams(vp, &ac->schedParams);
    
    if (ac->isUser) {
        vp->flags |= VP_FLAG_USER_OWNED;
    }
    else {
        vp->flags &= ~VP_FLAG_USER_OWNED;
    }
    vp->id = ac->id;
    vp->groupid = ac->groupid;
    vp->flags |= VP_FLAG_ACQUIRED;

    *pOutVP = vp;
    return EOK;


catch:
    if (doFree) {
        kfree(vp);
    }

    *pOutVP = NULL;
    return err;
}

// Relinquishes a virtual processor which means that it is finished executing
// code and that it should be moved back to the virtual processor pool. This
// function does not return to the caller.
_Noreturn vcpu_relinquish(vcpu_t _Nonnull self)
{
    // Cleanup
    vcpu_setdq(self, NULL, -1);
    self->proc = NULL;
    self->udata = 0;
    self->id = 0;
    self->groupid = 0;
    self->uerrno = 0;
    self->pending_sigs = 0;
    self->proc_sigs_enabled = 0;
    self->suspension_inhibit_count = 0;
    self->flags &= ~(VP_FLAG_USER_OWNED|VP_FLAG_HANDLING_EXCPT|VP_FLAG_ACQUIRED);


    // Check ourselves back into the vcpu pool
    const bool reused = vcpu_pool_checkin(g_vcpu_pool, self);


    // Suspend ourselves if the pool accepted us; otherwise terminate ourselves
    if (reused) {
        try_bang(vcpu_suspend(self));
    }
    else {
        sched_terminate_vcpu(g_sched, self);
    }
    /* NOT REACHED */
}

void vcpu_destroy(vcpu_t _Nullable self)
{
    if (self) {
        stk_destroy(&self->kernel_stack);
        stk_destroy(&self->user_stack);
        kfree(self);
    }
}

// Sets the dispatch queue that has acquired the virtual processor and owns it
// until the virtual processor is relinquished back to the virtual processor
// pool.
void vcpu_setdq(vcpu_t _Nonnull self, void* _Nullable pQueue, int concurrencyLaneIndex)
{
    self->dispatchQueue = pQueue;
    self->dispatchQueueConcurrencyLaneIndex = concurrencyLaneIndex;
}

void vcpu_reduce_sched_penalty(vcpu_t _Nonnull self, int prop)
{
    if (self->priority_bias < 0) {
        const int bias = self->priority_bias + prop;

        if (bias < 0) {
            self->priority_bias = bias;
        }
        else {
            self->priority_bias = 0;
        }

        vcpu_sched_params_changed(self);
    }
}

void vcpu_sched_params_changed(vcpu_t _Nonnull self)
{
    int sched_pri, eff_pri;

    if (self->qos > SCHED_QOS_IDLE) {
        sched_pri = ((self->qos - 1) << QOS_PRI_SHIFT) + (self->qos_priority - QOS_PRI_LOWEST) + 1;
        eff_pri = sched_pri + self->priority_bias;
        eff_pri = __max(__min(eff_pri, SCHED_PRI_HIGHEST), SCHED_PRI_LOWEST + 1);
    }
    else {
        // SCHED_QOS_IDLE has only one priority level
        eff_pri = SCHED_PRI_LOWEST;
    }

    assert(eff_pri >= SCHED_PRI_LOWEST && eff_pri <= SCHED_PRI_HIGHEST);

    self->sched_priority = (uint8_t)sched_pri;
    self->effective_priority = (uint8_t)eff_pri;
}

errno_t vcpu_getschedparams(vcpu_t _Nonnull self, int type, sched_params_t* _Nonnull params)
{
    decl_try_err();
    const int sps = preempt_disable();

    switch (type) {
        case SCHED_PARAM_QOS:
            params->type = SCHED_PARAM_QOS;
            params->u.qos.category = self->qos;
            params->u.qos.priority = self->qos_priority;
            break;

        default:
            err = EINVAL;
            break;
    }
    preempt_restore(sps);

    return err;
}

errno_t vcpu_setschedparams(vcpu_t _Nonnull self, const sched_params_t* _Nonnull params)
{
    decl_try_err();

    if (params->type != SCHED_PARAM_QOS) {
        return EINVAL;
    }
    if (params->u.qos.category < SCHED_QOS_BACKGROUND || params->u.qos.category > SCHED_QOS_REALTIME) {
        return EINVAL;
    }
    if (params->u.qos.priority < QOS_PRI_LOWEST || params->u.qos.priority > QOS_PRI_HIGHEST) {
        return EINVAL;
    }


    const int sps = preempt_disable();
    
    if (self->qos != params->u.qos.category || self->qos_priority != params->u.qos.priority) {
        switch (self->sched_state) {
            case SCHED_STATE_INITIATED:
                self->qos = params->u.qos.category;
                self->qos_priority = params->u.qos.priority;
                vcpu_sched_params_changed(self);
                break;

            case SCHED_STATE_READY:
                sched_set_unready(g_sched, self);
                self->qos = params->u.qos.category;
                self->qos_priority = params->u.qos.priority;
                vcpu_sched_params_changed(self);
                sched_set_ready(g_sched, self, true);
                break;
                
            case SCHED_STATE_RUNNING:
            case SCHED_STATE_WAITING:
            case SCHED_STATE_SUSPENDED:
            case SCHED_STATE_WAIT_SUSPENDED:
                self->qos = params->u.qos.category;
                self->qos_priority = params->u.qos.priority;
                if (self->sched_state == SCHED_STATE_RUNNING) {
                    self->quantum_countdown = qos_quantum(self->qos);
                }
                vcpu_sched_params_changed(self);
                break;

            case SCHED_STATE_TERMINATING:
                err = ESRCH;
                break;

            default:
                abort();
        }
    }
    preempt_restore(sps);

    return err;
}

int vcpu_getcurrentpriority(vcpu_t _Nonnull self)
{
    const int sps = preempt_disable();
    const uint8_t pri = self->effective_priority;
    preempt_restore(sps);

    return (int)pri;
}

// Yields the remainder of the current quantum to other VPs.
void vcpu_yield(void)
{
    const int sps = preempt_disable();
    vcpu_t self = (vcpu_t)g_sched->running;

    if (self->sched_state == SCHED_STATE_RUNNING) {
        if (self->priority_bias < 0) {
            vcpu_reduce_sched_penalty(self, -self->priority_bias / 2);
        }

        sched_switch_to(g_sched, sched_highest_priority_ready(g_sched));
    }
    preempt_restore(sps);
}

// Suspends the calling virtual processor if possible at this time. This function
// supports nested calls. A VP may only be suspended while it is executing in a
// "suspension safe zone". EBUSY is returned if the VP is not in such a zone at
// the time this function is called. Suspension safe zones are:
// * target VP is already suspended
// * target VP executes in user space
// * target VP executes in kernel space AND it is not currently inside a scope
//   bracketed by a disable_suspensions() and enable_suspensions() pair
//
// Note that involuntary suspension of a VP is not supported for kernel owned
// VPs.
//
// Note that the implementation of this calls relies on the fact that the VP that
// calls vcpu_suspend() and the target VP can not run at the same time. This is
// easy to guarantee on a single core machine but requires on a multi-core
// machine that the actual suspension is done on the same core on which the
// target VP lives. Eg if the vcpu_suspend() VP is on core A and the target VP
// is on core B then A needs to send a message to the scheduler VP on B so that
// the scheduler VP on core B then does the actual suspension job.
errno_t vcpu_suspend(vcpu_t _Nonnull self)
{
    decl_try_err();
    const int sps = preempt_disable();

    if (self->sched_state == SCHED_STATE_TERMINATING || self == g_sched->idle_vp || self == g_sched->boot_vp) {
        throw(ESRCH);
    }
    if (self->suspension_count == INT8_MAX) {
        throw(EINVAL);
    }
    if ((self->flags & VP_FLAG_USER_OWNED) == 0 && (self->sched_state != SCHED_STATE_INITIATED && self->sched_state != SCHED_STATE_RUNNING)) {
        // no involuntary suspend of kernel owned VPs
        throw(EPERM);
    }
    if (self->suspension_inhibit_count > 0) {
        // target VP is executing inside a suspension exclusion zone
        throw(EBUSY);
    }
    
    const int old_state = self->sched_state;
    self->sched_state = (old_state == SCHED_STATE_WAITING) ? SCHED_STATE_WAIT_SUSPENDED : SCHED_STATE_SUSPENDED;
    self->suspension_count++;
    
    if (self->suspension_count == 1) {
        self->suspension_time = clock_getticks(g_mono_clock);

        switch (old_state) {
            case SCHED_STATE_INITIATED:
                break;

            case SCHED_STATE_READY:
                sched_set_unready(g_sched, self);
                break;
            
            case SCHED_STATE_RUNNING:
                // We're running, thus we are not on the ready queue. Do a forced
                // context switch to some other VP.
                sched_switch_to(g_sched, sched_highest_priority_ready(g_sched));
                break;
            
            case SCHED_STATE_WAITING:
                wq_suspendone(self->waiting_on_wait_queue, self);
                break;
            
            default:
                abort();
        }
    }
    
catch:
    preempt_restore(sps);

    return err;
}

// Resumes the given virtual processor. The virtual processor is forcefully
// resumed if 'force' is true. This means that it is resumed even if the suspension
// count is > 1.
void vcpu_resume(vcpu_t _Nonnull self, bool force)
{
    const int sps = preempt_disable();
    
    if (self->sched_state == SCHED_STATE_SUSPENDED || self->sched_state == SCHED_STATE_WAIT_SUSPENDED) {
        if (force) {
            self->suspension_count = 0;
        }
        else if (self->suspension_count > 0) {
            self->suspension_count--;
        }


        if (self->suspension_count == 0) {
            if (self->sched_state == SCHED_STATE_SUSPENDED) {
                if (self->priority_bias < 0) {
                    vcpu_reduce_sched_penalty(self, -self->priority_bias);
                }
                sched_set_ready(g_sched, self, true);
            }
            else {
                self->sched_state = SCHED_STATE_WAITING;
                wq_resumeone(self->waiting_on_wait_queue, self);
            }
        }
    }
    preempt_restore(sps);
}

// Returns true if the given virtual processor is currently suspended; false otherwise.
bool vcpu_suspended(vcpu_t _Nonnull self)
{
    const int sps = preempt_disable();
    const bool isSuspended = self->sched_state == SCHED_STATE_SUSPENDED || self->sched_state == SCHED_STATE_WAIT_SUSPENDED;
    preempt_restore(sps);
    return isSuspended;
}

vcpuid_t new_vcpu_groupid(void)
{
    static vcpuid_t id = VCPUID_MAIN_GROUP;

    return AtomicInt_Increment((volatile AtomicInt*)&id);
}
