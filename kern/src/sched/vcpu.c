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
#include <assert.h>
#include <limits.h>
#include <string.h>
#include <ext/atomic.h>
#include <ext/math.h>
#include <ext/timespec.h>
#include <hal/clock.h>
#include <hal/sched.h>
#include <kern/kernlib.h>
#include <kern/kalloc.h>

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
    assert(sched_params->u.qos.category >= SCHED_QOS_BACKGROUND && sched_params->u.qos.category <= SCHED_QOS_REALTIME);
    assert(sched_params->u.qos.priority >= QOS_PRI_LOWEST && sched_params->u.qos.priority <= QOS_PRI_HIGHEST);


    memset(self, 0, sizeof(struct vcpu));
    stk_init(&self->kernel_stack);
    stk_init(&self->user_stack);
    
    self->timeout = CLOCK_DEADLINE_INIT;    
    self->sched_state = SCHED_STATE_INITIATED;

    self->flags = 0;
    self->flags |= (g_sys_desc->fpu_model > FPU_MODEL_NONE) ? VP_FLAG_HAS_FPU : 0;
    self->flags |= (g_sys_desc->cpu_model == CPU_MODEL_68060) ? VP_FLAG_HAS_BC : 0;
    self->base_priority = SCHED_PRI_FROM_QOS(sched_params->u.qos.category, sched_params->u.qos.priority);

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
    vcpu_reset_quantum(vp);
    
    if (ac->isUser) {
        vp->flags |= VP_FLAG_USER_OWNED;
    }
    else {
        vp->flags &= ~VP_FLAG_USER_OWNED;
    }
    vp->id = ac->id;
    vp->groupid = ac->groupid;
    vp->flags &= ~VP_FLAG_DID_WAIT;
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
_Noreturn void vcpu_relinquish(vcpu_t _Nonnull self)
{
    decl_try_err();
    assert(vcpu_current() == self);
    
    // Reset priority penalty and boost
    const int sps = preempt_disable();
    self->priority_boost = 0;
    self->priority_penalty = 0;
    self->quantum_boost = 0;
    vcpu_sched_params_changed(self);
    preempt_restore(sps);


    // Cleanup
    self->dispatch_worker = NULL;
    self->proc = NULL;
    self->udata = 0;
    self->id = 0;
    self->groupid = 0;
    self->uerrno = 0;
    self->pending_sigs = 0;
    self->excpt_handler = (excpt_handler_t){0};
    self->excpt_handler_flags = 0;
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

void vcpu_set_quantum_boost(vcpu_t _Nonnull self, int boost)
{
    self->quantum_boost = __max(__min(boost, INT8_MAX), 0);
}

void vcpu_sched_params_changed(vcpu_t _Nonnull self)
{
    const int base_qos_class = SCHED_QOS_CLASS(self->base_priority);
    const int base_qos_pri = SCHED_QOS_PRI(self->base_priority);
    int eff_pri;

    self->flags &= ~VP_FLAG_FIXED_PRI;
    if ((base_qos_class == SCHED_QOS_BACKGROUND && base_qos_pri == QOS_PRI_LOWEST) || (base_qos_class == SCHED_QOS_REALTIME)) {
        self->flags |= VP_FLAG_FIXED_PRI;
    }


    if (vcpu_is_fixed_pri(self)) {
        // Fixed priority
        eff_pri = self->base_priority;
    }
    else {
        // Dynamic priority
        const int pri_boost = (self->priority_penalty <= 0) ? self->priority_boost : 0;
        const int boosted_qos_pri = __min(base_qos_pri + pri_boost, QOS_PRI_HIGHEST);
        const int boosted_pri = SCHED_PRI_FROM_QOS(base_qos_class, boosted_qos_pri);
        const int dyn_top_pri = SCHED_QOS_URGENT * QOS_PRI_COUNT - 1;
        const int dyn_bot_pri = SCHED_PRI_LOWEST + 1;

        eff_pri = boosted_pri - self->priority_penalty;
        eff_pri = __max(__min(eff_pri, dyn_top_pri), dyn_bot_pri);
    }

//    assert(eff_pri >= SCHED_PRI_LOWEST && eff_pri <= SCHED_PRI_HIGHEST);

    self->effective_priority = (int8_t)eff_pri;
}

void vcpu_reset_quantum(vcpu_t _Nonnull self)
{
    register const int8_t qos_class = SCHED_QOS_CLASS(self->effective_priority);
    register const int8_t base_len = g_quantum_base_length[qos_class];
    register const int8_t max_len = g_quantum_max_length[qos_class];

    self->quantum_countdown = __min(base_len + self->quantum_boost, max_len);
}

errno_t vcpu_getschedparams(vcpu_t _Nonnull self, int type, sched_params_t* _Nonnull params)
{
    decl_try_err();
    const int sps = preempt_disable();

    switch (type) {
        case SCHED_PARAM_QOS:
            params->type = SCHED_PARAM_QOS;
            params->u.qos.category = SCHED_QOS_CLASS(self->base_priority);
            params->u.qos.priority = SCHED_QOS_PRI(self->base_priority);
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
    if (params->u.qos.category == SCHED_QOS_BACKGROUND && params->u.qos.priority == QOS_PRI_LOWEST) {
        // Reserved for scheduler 
        return EPERM;
    }
    if (params->u.qos.category == SCHED_QOS_REALTIME && params->u.qos.priority == QOS_PRI_HIGHEST) {
        // Reserved for scheduler 
        return EPERM;
    }


    const int new_base_pri = SCHED_PRI_FROM_QOS(params->u.qos.category, params->u.qos.priority);
    const int sps = preempt_disable();
    
    if (self->base_priority != new_base_pri) {
        switch (self->sched_state) {
            case SCHED_STATE_INITIATED:
                self->base_priority = new_base_pri;
                vcpu_sched_params_changed(self);
                break;

            case SCHED_STATE_READY:
                sched_set_unready(g_sched, self, false);
                self->base_priority = new_base_pri;
                vcpu_sched_params_changed(self);
                sched_set_ready(g_sched, self, true);
                break;
                
            case SCHED_STATE_RUNNING:
            case SCHED_STATE_WAITING:
            case SCHED_STATE_SUSPENDED:
                self->base_priority = new_base_pri;
                if (self->sched_state == SCHED_STATE_RUNNING) {
                    vcpu_reset_quantum(self);
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
        if (!vcpu_is_fixed_pri(self) && self->priority_penalty > 0) {
            // Half the priority penalty, if any
            self->priority_penalty /= 2;
            vcpu_sched_params_changed(self);
        }
        self->flags |= VP_FLAG_DID_WAIT;

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

errno_t vcpu_set_excpt_handler(vcpu_t _Nonnull self, int flags, const excpt_handler_t* _Nullable handler, excpt_handler_t* _Nullable old_handler)
{
    if ((flags & ~EXCPT_MCTX) != 0 || (handler->func == NULL && flags != 0)) {
        return EINVAL;
    }

    if (old_handler) {
        *old_handler = self->excpt_handler;
    }
    self->excpt_handler = *handler;
    self->excpt_handler_flags = flags;

    return EOK;
}

vcpuid_t new_vcpu_groupid(void)
{
    static atomic_int id = VCPUID_MAIN_GROUP;

    return (vcpuid_t)atomic_int_fetch_add(&id, 1);
}
