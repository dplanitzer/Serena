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

static void _vcpu_yield(vcpu_t _Nonnull self);


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
    const int sps = preempt_disable();
    while (vp->sched_state != SCHED_STATE_SUSPENDED) {
        _vcpu_yield(vcpu_current());
    }
    preempt_restore(sps);

    
    // Configure the vcpu
    try(_vcpu_reset_mcontext(vp, ac, true));
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
    assert(vcpu_current() == self);
    
    // Cleanup
    self->proc = NULL;
    self->udata = 0;
    self->id = 0;
    self->groupid = 0;
    self->uerrno = 0;
    self->pending_sigs = 0;
    self->excpt_id = 0;
    self->excpt_sa = NULL;
    self->syscall_sa = NULL;
    self->flags &= ~(VP_FLAG_USER_OWNED|VP_FLAG_ACQUIRED);


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
                sched_set_unready(g_sched, self, false);
                self->qos = params->u.qos.category;
                self->qos_priority = params->u.qos.priority;
                vcpu_sched_params_changed(self);
                sched_set_ready(g_sched, self, true);
                break;
                
            case SCHED_STATE_RUNNING:
            case SCHED_STATE_WAITING:
            case SCHED_STATE_SUSPENDED:
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

// @Entry Condition: preemption disabled
static void _vcpu_yield(vcpu_t _Nonnull self)
{
    if (self->sched_state == SCHED_STATE_RUNNING) {
        if (self->priority_bias < 0) {
            vcpu_reduce_sched_penalty(self, -self->priority_bias / 2);
        }

        sched_switch_to(g_sched, sched_highest_priority_ready(g_sched));
    }
}

// Yields the remainder of the current quantum to other VPs.
void vcpu_yield(void)
{
    const int sps = preempt_disable();
    vcpu_t self = (vcpu_t)g_sched->running;

    _vcpu_yield(self);
    preempt_restore(sps);
}

errno_t vcpu_suspend(vcpu_t _Nonnull self)
{
    decl_try_err();
    const int sps = preempt_disable();

    if (self->sched_state == SCHED_STATE_TERMINATING || self == g_sched->idle_vp || self == g_sched->boot_vp) {
        throw(ESRCH);
    }
    if ((self->flags & VP_FLAG_USER_OWNED) == 0 && (self->sched_state != SCHED_STATE_INITIATED && self->sched_state != SCHED_STATE_RUNNING)) {
        // no involuntary suspension of kernel owned VPs
        throw(EPERM);
    }
    if (self->suspension_count == INT16_MAX) {
        throw(EINVAL);
    }


    if (self->sched_state == SCHED_STATE_SUSPENDED || (self->pending_sigs & _SIGBIT(SIGVPDS)) != 0) {
        // 'self' has at least a suspension request pending or may already have entered suspended state
        self->suspension_count++;
    }
    else if (self->sched_state == SCHED_STATE_INITIATED) {
        // 'self' was just created. Move it to suspended state immediately
        self->suspension_count++;
        self->sched_state = SCHED_STATE_SUSPENDED;
    }
    else if (vcpu_current() == self) {
        // 'self' is currently running. Move it to suspended state immediately
        self->suspension_count++;
        self->sched_state = SCHED_STATE_SUSPENDED;
        sched_switch_to(g_sched, sched_highest_priority_ready(g_sched));
    }
    else {
        // 'self' is some other vcpu in some state (running, ready, waiting). Trigger a deferred suspend on it
        self->suspension_count++;
        vcpu_sigsend(self, SIGVPDS);
    }

catch:
    preempt_restore(sps);

    return err;
}

void vcpu_do_pending_deferred_suspend(vcpu_t _Nonnull self)
{
    const int sps = preempt_disable();

    // This function is always called in running state and thus the transition
    // running -> suspended is the only one we need to handle.
    // Atomically check and consume the SIGVPDS signal after we've changed our
    // state to suspended to ensure that there's no gap between suspension
    // request and suspension state.
    // Note that it is crucial that we check the SIGVPDS flag, consume it and
    // change our scheduler state to SCHED_STATE_SUSPENDED in an atomic
    // operation to ensure that vcpu_resume() can not see the transition and
    // get potentially confused by it.
    if ((self->pending_sigs & _SIGBIT(SIGVPDS)) != 0) {
        self->sched_state = SCHED_STATE_SUSPENDED;
        self->pending_sigs &= ~_SIGBIT(SIGVPDS);

        sched_switch_to(g_sched, sched_highest_priority_ready(g_sched));
    }
    
    preempt_restore(sps);
}

// @Entry Condition: preemption disabled
static void _vcpu_resume(vcpu_t _Nonnull self, bool force)
{
    if (force) {
        self->suspension_count = 0;
    }
    else if (self->suspension_count > 0) {
        self->suspension_count--;
    }


    if (self->suspension_count == 0) {
        if (self->priority_bias < 0) {
            vcpu_reduce_sched_penalty(self, -self->priority_bias);
        }
        sched_set_ready(g_sched, self, true);
    }
}

void vcpu_resume(vcpu_t _Nonnull self, bool force)
{
    const int sps = preempt_disable();

    // Cancel a pending deferred suspend request. This is safe because
    // vcpu_do_pending_deferred_suspend() atomically tests and acts on the flag
    self->pending_sigs &= ~_SIGBIT(SIGVPDS);


    // Move the vcpu out of suspended state if it is suspended
    if (self->sched_state == SCHED_STATE_SUSPENDED) {
        _vcpu_resume(self, force);
    }

    preempt_restore(sps);
}

errno_t vcpu_rw_mcontext(vcpu_t _Nonnull self, mcontext_t* _Nonnull ctx, bool isRead)
{
    decl_try_err();
    const int sps = preempt_disable();

    for (;;) {
        if (self->sched_state == SCHED_STATE_SUSPENDED || (self->sched_state == SCHED_STATE_WAITING && (self->pending_sigs & _SIGBIT(SIGVPDS)) != 0)) {
            // Wait until the target vcpu has entered suspended state or it is
            // waiting and has a deferred suspension request pending.
            break;
        }

        if ((self->pending_sigs & _SIGBIT(SIGVPDS)) == 0) {
            // The target vcpu isn't suspended and doesn't even have a deferred
            // suspension request pending. Won't be able to r/w its ucontext
            err = EBUSY;
            break;
        }

        _vcpu_yield(vcpu_current());
    }

    
    if (err == EOK) {
        if (isRead) {
            _vcpu_read_mcontext(self, ctx);
        }
        else {
            _vcpu_write_mcontext(self, ctx);
        }
    }

    preempt_restore(sps);

    return err;
}

vcpuid_t new_vcpu_groupid(void)
{
    static vcpuid_t id = VCPUID_MAIN_GROUP;

    return AtomicInt_Increment((volatile AtomicInt*)&id);
}
