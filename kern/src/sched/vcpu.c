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
#include <ext/nanotime.h>
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
void vcpu_init(vcpu_t _Nonnull self, const vcpu_policy_t* _Nonnull policy)
{
    assert(policy->version == sizeof(vcpu_policy_t));
    assert(policy->qos.grade >= VCPU_QOS_BACKGROUND && policy->qos.grade <= VCPU_QOS_REALTIME);
    assert(policy->qos.priority >= VCPU_PRI_LOWEST && policy->qos.priority <= VCPU_PRI_HIGHEST);


    memset(self, 0, sizeof(struct vcpu));
    stk_init(&self->kernel_stack);
    stk_init(&self->user_stack);
    
    self->timeout = CLOCK_DEADLINE_INIT;    
    self->run_state = VCPU_RUST_INITIATED;

    self->flags = 0;
    self->flags |= (cpu_68k_fpu(g_sys_desc->cpu_subtype) != CPU_FPU_NONE) ? VP_FLAG_HAS_FPU : 0;
    self->flags |= (cpu_68k_family(g_sys_desc->cpu_subtype) == CPU_FAMILY_68060) ? VP_FLAG_HAS_BC : 0;
    self->base_priority = SCHED_PRI_FROM_QOS(policy->qos.grade, policy->qos.priority);

    vcpu_on_sched_param_changed(self);
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

        vcpu_init(vp, &ac->policy);
        vcpu_suspend(vp);
    }


    // Note that a vcpu freshly checked out from the pool may not have entered
    // the suspend state yet. Wait until it is actually suspended and before we
    // proceed with reconfiguring it. We only become the owner of the vcpu once
    // it has entered suspended state.
    const int sps = preempt_disable();
    while (vp->run_state != VCPU_RUST_SUSPENDED) {
        _vcpu_yield(vcpu_current());
    }
    preempt_restore(sps);

    
    // Configure the vcpu
    try(_vcpu_reset_machine_state(vp, ac, true));
    vcpu_set_policy(vp, &ac->policy);
    vcpu_reset_quantum(vp);
    
    if (ac->isUser) {
        vp->flags |= VP_FLAG_USER_OWNED;
    }
    else {
        vp->flags &= ~VP_FLAG_USER_OWNED;
    }
    vp->id = ac->id;
    vp->group_id = ac->group_id;
    vp->flags &= ~VP_FLAG_DID_WAIT;
    vp->flags |= VP_FLAG_ACQUIRED;

    vp->user_ticks = 0;
    vp->system_ticks = 0;
    vp->wait_ticks = 0;
    clock_gettime(g_mono_clock, &vp->acquisition_time);

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
    self->sched_nice = 0;
    vcpu_on_sched_param_changed(self);
    preempt_restore(sps);


    // Cleanup
    self->dispatch_worker = NULL;
    self->proc = NULL;
    self->udata = 0;
    self->id = 0;
    self->group_id = 0;
    self->uerrno = 0;
    self->pending_sigs = 0;
    self->excpt_handler = (excpt_handler_t){0};
    self->excpt_handler_flags = 0;
    self->excpt_state = (cpu_excpt_state_t){0};
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

void vcpu_set_nice(vcpu_t _Nonnull self, int nice)
{
    const int new_nice = __max(__min(nice, VCPU_QOS_URGENT * VCPU_PRI_COUNT), 0);

    if (self->sched_nice != new_nice) {
        self->sched_nice = new_nice;
        if (self->sched_nice > 0) {
            // we give this guy a second chance
            self->priority_penalty = 0;
        }

        vcpu_on_sched_param_changed(self);
    }
}

void vcpu_on_sched_param_changed(vcpu_t _Nonnull self)
{
    const int base_qos_class = SCHED_QOS_GRADE(self->base_priority);
    const int base_qos_pri = SCHED_QOS_PRI(self->base_priority);
    int eff_pri;

    self->flags &= ~VP_FLAG_FIXED_PRI;
    if ((base_qos_class == VCPU_QOS_BACKGROUND && base_qos_pri == VCPU_PRI_LOWEST) || (base_qos_class == VCPU_QOS_REALTIME)) {
        self->flags |= VP_FLAG_FIXED_PRI;
    }


    if (vcpu_is_fixed_pri(self)) {
        // Fixed priority
        eff_pri = self->base_priority;
    }
    else {
        // Dynamic priority
        const int pri_boost = (self->priority_penalty <= 0) ? self->priority_boost : 0;
        const int boosted_qos_pri = __min(base_qos_pri + pri_boost, VCPU_PRI_HIGHEST);
        const int boosted_pri = SCHED_PRI_FROM_QOS(base_qos_class, boosted_qos_pri);
        const int dyn_top_pri = VCPU_QOS_URGENT * VCPU_PRI_COUNT - 1;
        const int dyn_bot_pri = SCHED_PRI_LOWEST + 1;

        eff_pri = boosted_pri - self->priority_penalty - self->sched_nice;
        eff_pri = __max(__min(eff_pri, dyn_top_pri), dyn_bot_pri);
    }

//    assert(eff_pri >= SCHED_PRI_LOWEST && eff_pri <= SCHED_PRI_HIGHEST);

    self->cur_priority = (int8_t)eff_pri;
}

static int8_t _vcpu_effective_quantum_length(vcpu_t _Nonnull self)
{
    register const int8_t qos_class = SCHED_QOS_GRADE(self->cur_priority);
    register const int8_t base_len = g_quantum_base_length[qos_class];
    register const int8_t max_len = g_quantum_max_length[qos_class];

    return __min(base_len + self->quantum_boost, max_len);
}

void vcpu_reset_quantum(vcpu_t _Nonnull self)
{
    self->quantum_countdown = _vcpu_effective_quantum_length(self);
}

errno_t vcpu_policy(vcpu_t _Nonnull self, int version, vcpu_policy_t* _Nonnull policy)
{
    if (version != sizeof(vcpu_policy_t)) {
        return EINVAL;
    }

    const int sps = preempt_disable();
    policy->version = sizeof(vcpu_policy_t);
    policy->qos.grade = SCHED_QOS_GRADE(self->base_priority);
    policy->qos.priority = SCHED_QOS_PRI(self->base_priority);
    preempt_restore(sps);

    return EOK;
}

errno_t vcpu_set_policy(vcpu_t _Nonnull self, const vcpu_policy_t* _Nonnull policy)
{
    decl_try_err();

    if (policy->version != sizeof(vcpu_policy_t)) {
        return EINVAL;
    }
    if (policy->qos.grade < VCPU_QOS_BACKGROUND || policy->qos.grade > VCPU_QOS_REALTIME) {
        return EINVAL;
    }
    if (policy->qos.priority < VCPU_PRI_LOWEST || policy->qos.priority > VCPU_PRI_HIGHEST) {
        return EINVAL;
    }
    if (policy->qos.grade == VCPU_QOS_BACKGROUND && policy->qos.priority == VCPU_PRI_LOWEST) {
        // Reserved for scheduler 
        return EPERM;
    }
    if (policy->qos.grade == VCPU_QOS_REALTIME && policy->qos.priority == VCPU_PRI_HIGHEST) {
        // Reserved for scheduler 
        return EPERM;
    }


    const int new_base_pri = SCHED_PRI_FROM_QOS(policy->qos.grade, policy->qos.priority);
    const int sps = preempt_disable();
    
    if (self->base_priority != new_base_pri) {
        switch (self->run_state) {
            case VCPU_RUST_INITIATED:
                self->base_priority = new_base_pri;
                vcpu_on_sched_param_changed(self);
                break;

            case VCPU_RUST_READY:
                sched_set_unready(g_sched, self, false);
                self->base_priority = new_base_pri;
                vcpu_on_sched_param_changed(self);
                sched_set_ready(g_sched, self, true);
                break;
                
            case VCPU_RUST_RUNNING:
            case VCPU_RUST_WAITING:
            case VCPU_RUST_SUSPENDED:
                self->base_priority = new_base_pri;
                if (self->run_state == VCPU_RUST_RUNNING) {
                    vcpu_reset_quantum(self);
                }
                vcpu_on_sched_param_changed(self);
                break;

            case VCPU_RUST_TERMINATING:
                err = ESRCH;
                break;

            default:
                abort();
        }
    }
    preempt_restore(sps);

    return err;
}

// @Entry Condition: preemption disabled
static void _vcpu_yield(vcpu_t _Nonnull self)
{
    if (self->run_state == VCPU_RUST_RUNNING) {
        if (!vcpu_is_fixed_pri(self) && self->priority_penalty > 0) {
            // Half the priority penalty, if any
            self->priority_penalty /= 2;
            vcpu_on_sched_param_changed(self);
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

    if (self->run_state == VCPU_RUST_TERMINATING || self == g_sched->idle_vp || self == g_sched->boot_vp) {
        throw(ESRCH);
    }
    if ((self->flags & VP_FLAG_USER_OWNED) == 0 && (self->run_state != VCPU_RUST_INITIATED && self->run_state != VCPU_RUST_RUNNING)) {
        // no involuntary suspension of kernel owned VPs
        throw(EPERM);
    }
    if (self->suspension_count == INT16_MAX) {
        throw(EINVAL);
    }


    if (self->run_state == VCPU_RUST_SUSPENDED || (self->pending_sigs & sig_bit(SIG_VCPU_SUSPEND)) != 0) {
        // 'self' has at least a suspension request pending or may already have entered suspended state
        self->suspension_count++;
    }
    else if (self->run_state == VCPU_RUST_INITIATED) {
        // 'self' was just created. Move it to suspended state immediately
        self->suspension_count++;
        self->run_state = VCPU_RUST_SUSPENDED;
    }
    else if (vcpu_current() == self) {
        // 'self' is currently running. Move it to suspended state immediately
        self->suspension_count++;
        self->run_state = VCPU_RUST_SUSPENDED;
        sched_switch_to(g_sched, sched_highest_priority_ready(g_sched));
    }
    else {
        // 'self' is some other vcpu in some state (running, ready, waiting). Trigger a deferred suspend on it
        self->suspension_count++;
        vcpu_send_signal(self, SIG_VCPU_SUSPEND);
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
    // Atomically check and consume the SIG_VCPU_SUSPEND signal after we've changed our
    // state to suspended to ensure that there's no gap between suspension
    // request and suspension state.
    // Note that it is crucial that we check the SIG_VCPU_SUSPEND flag, consume it and
    // change our scheduler state to VCPU_RUST_SUSPENDED in an atomic
    // operation to ensure that vcpu_resume() can not see the transition and
    // get potentially confused by it.
    if ((self->pending_sigs & sig_bit(SIG_VCPU_SUSPEND)) != 0) {
        self->run_state = VCPU_RUST_SUSPENDED;
        self->pending_sigs &= ~sig_bit(SIG_VCPU_SUSPEND);

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
    self->pending_sigs &= ~sig_bit(SIG_VCPU_SUSPEND);


    // Move the vcpu out of suspended state if it is suspended
    if (self->run_state == VCPU_RUST_SUSPENDED) {
        _vcpu_resume(self, force);
    }

    preempt_restore(sps);
}

// Checks whether 'self' is in the process of being suspended or is suspended,
// waits for suspension to have completed and returns EOK. Returns EBUSY if
// 'self' is not in process suspension and not suspended either. 
// @Entry Condition: preemption disabled
static errno_t _vcpu_await_suspension(vcpu_t _Nonnull self)
{
    for (;;) {
        if (self->run_state == VCPU_RUST_SUSPENDED || (self->run_state == VCPU_RUST_WAITING && (self->pending_sigs & sig_bit(SIG_VCPU_SUSPEND)) != 0)) {
            // Wait until the target vcpu has entered suspended state or it is
            // waiting and has a deferred suspension request pending.
            break;
        }

        if ((self->pending_sigs & sig_bit(SIG_VCPU_SUSPEND)) == 0) {
            // The target vcpu isn't suspended and doesn't even have a deferred
            // suspension request pending. Won't be able to r/w its ucontext
            return EBUSY;
        }

        _vcpu_yield(vcpu_current());
    }

    return EOK;
}

// Returns a copy of the receiver's CPU state.
errno_t vcpu_state(vcpu_t _Nonnull self, int flavor, vcpu_state_ref _Nonnull state)
{
    decl_try_err();
    const bool is_running = (self == vcpu_current()) ? true : false;
    const cpu_basic_state_t* bp;
    const cpu_float_state_t* fp;
    int sps;

    if (_VCPU_IS_EXCPT_FLAVOR(flavor)) {
        if (!vcpu_is_handling_exception(self)) {
            return ESRCH;
        }

        flavor = _VCPU_STRIP_EXCPT_FLAVOR(flavor);
        bp = &self->excpt_sa->b;
        fp = &self->excpt_sa->f;
    }
    else {
        bp = (is_running) ? self->syscall_sa : &self->csw_sa->b;
        fp = (is_running) ? NULL : &self->csw_sa->f;
    }


    // Must be suspended if we are not the running vcpu
    if (!is_running) {
        sps = preempt_disable();
        err = _vcpu_await_suspension(self);
    }

    if (err == EOK) {
        switch (flavor) {
            case VCPU_STATE_M68K:
                _cpu_get_basic_state(state, bp);
                break;

            case VCPU_STATE_M68K_FLOAT:
                if ((self->flags & VP_FLAG_HAS_FPU) == VP_FLAG_HAS_FPU) {
                    if (fp) {
                        _cpu_get_float_state(state, fp);
                    }
                    else {
                        // The kernel doesn't use the FP registers and doesn't save them.
                        // Just read the directly from the CPU
                        _cpu_get_float_regs(state);
                    }
                }
                else {
                    err = ENOTSUP;
                }
                break;

            case VCPU_STATE_EXCPT: {
                vcpu_state_excpt_t* esp = state;

                esp->code = self->excpt_state.code;
                esp->cpu_code = self->excpt_state.cpu_code;
                esp->sp = self->excpt_state.sp;
                esp->pc = self->excpt_state.pc;
                esp->fault_addr = self->excpt_state.fault_addr;
                break;
            }

            default:
                err = EINVAL;
                break;
        }
    }

    if (!is_running) {
        preempt_restore(sps);
    }

    return err;
}

// Updates the CPU state of the receiver. If the receiver is not in running state,
// then it must be in suspended state.
errno_t vcpu_set_state(vcpu_t _Nonnull self, int flavor, const vcpu_state_ref _Nonnull state)
{
    decl_try_err();
    const bool is_running = (self == vcpu_current()) ? true : false;
    cpu_basic_state_t* bp;
    cpu_float_state_t* fp;
    int sps;

    if (_VCPU_IS_EXCPT_FLAVOR(flavor)) {
        if (!vcpu_is_handling_exception(self)) {
            return ESRCH;
        }

        flavor = _VCPU_STRIP_EXCPT_FLAVOR(flavor);
        bp = &self->excpt_sa->b;
        fp = &self->excpt_sa->f;
    }
    else {
        bp = (is_running) ? self->syscall_sa : &self->csw_sa->b;
        fp = (is_running) ? NULL : &self->csw_sa->f;
    }


    // Must be suspended if we are not the running vcpu
    if (!is_running) {
        sps = preempt_disable();
        err = _vcpu_await_suspension(self);
    }

    if (err == EOK) {
        switch (flavor) {
            case VCPU_STATE_M68K:
                _cpu_set_basic_state(bp, state);
                break;

            case VCPU_STATE_M68K_FLOAT:
                if ((self->flags & VP_FLAG_HAS_FPU) == VP_FLAG_HAS_FPU) {
                    if (fp) {
                        _cpu_set_float_state(fp, state);
                    }
                    else {
                        // The kernel doesn't use the FP registers and doesn't save them.
                        // Just write the directly to the CPU
                        _cpu_set_float_regs(state);
                    }
                }
                else {
                    err = ENOTSUP;
                }
                break;

            default:
                err = EINVAL;
                break;
        }
    }

    if (!is_running) {
        preempt_restore(sps);
    }

    return err;
}

// Returns a copy of the receiver's information.
errno_t vcpu_info(vcpu_t _Nonnull self, int flavor, vcpu_info_ref _Nonnull info)
{
    decl_try_err();
    const bool is_running = (self == vcpu_current()) ? true : false;
    int sps;

    // Must be suspended if we are not the running vcpu
    if (!is_running) {
        sps = preempt_disable();
    }

    switch (flavor) {
        case VCPU_INFO_STACK: {
            vcpu_stack_info_t* ip = info;

            if (vcpu_has_user_state(self)) {
                ip->base_ptr = self->user_stack.base;
                ip->size = self->user_stack.size;
            }
            else {
                err = ESRCH;
            }
            break;
        }

        case VCPU_INFO_SCHEDULING: {
            vcpu_scheduling_info_t* ip = info;

            ip->run_state = self->run_state;
            ip->base_qos.grade = SCHED_QOS_GRADE(self->base_priority);
            ip->base_qos.priority = SCHED_QOS_PRI(self->base_priority);
            ip->cur_qos.grade = SCHED_QOS_GRADE(self->cur_priority);
            ip->cur_qos.priority = SCHED_QOS_PRI(self->cur_priority);
            ip->base_quantum_length = g_quantum_base_length[ip->base_qos.grade];
            ip->cur_quantum_length = _vcpu_effective_quantum_length(self);
            ip->suspend_count = self->suspension_count;
            ip->flags = 0;
                
            if (self->priority_boost > 0)   ip->flags |= VCPU_HAS_PRIORITY_BOOST;
            if (self->quantum_boost > 0)    ip->flags |= VCPU_HAS_QUANTUM_BOOST;
            if (self->priority_penalty > 0) ip->flags |= VCPU_HAS_PRIORITY_PENALTY;
            break;
        }

        case VCPU_INFO_IDS: {
            vcpu_ids_info_t* ip = info;

            ip->id = self->id;
            ip->group_id = self->group_id;
            break;
        }

        case VCPU_INFO_TIMES: {
            vcpu_times_info_t* ip = info;

            clock_ticks2time(g_mono_clock, self->user_ticks, &ip->user_time);
            clock_ticks2time(g_mono_clock, self->system_ticks, &ip->system_time);
            clock_ticks2time(g_mono_clock, self->wait_ticks, &ip->wait_time);
            ip->acquisition_time = self->acquisition_time;
            break;
        }

        default:
            err = EINVAL;
            break;
    }

    if (!is_running) {
        preempt_restore(sps);
    }

    return err;
}

errno_t vcpu_set_excpt_handler(vcpu_t _Nonnull self, int flags, const excpt_handler_t* _Nullable handler, excpt_handler_t* _Nullable old_handler)
{
    if ((flags != 0) || (handler->func == NULL && flags != 0)) {
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
