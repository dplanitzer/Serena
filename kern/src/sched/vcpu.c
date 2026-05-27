//
//  vcpu.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/21/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "vcpu.h"
#include "sched.h"
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

static void _vcpu_reset_penalties_and_boosts_np(vcpu_t _Nonnull self);
static void _vcpu_set_base_priority_np(vcpu_t _Nonnull self, const vcpu_policy_t* _Nonnull policy);
static void vcpu_yield_np(vcpu_t _Nonnull self);
static errno_t vcpu_await_suspension_np(vcpu_t _Nonnull self);


void vcpu_init(vcpu_t _Nonnull self)
{
    memset(self, 0, sizeof(struct vcpu));
    stk_init(&self->kernel_stack);
    stk_init(&self->user_stack);
    
    self->timeout = CLOCK_DEADLINE_INIT;    
    self->run_state = VCPU_STATE_SUSPENDED;
    self->suspension_count = 1;

    self->flags = 0;
    self->flags |= (cpu_68k_fpu(g_sys_desc->cpu_subtype) != CPU_FPU_NONE) ? VP_FLAG_HAS_FPU : 0;
    self->flags |= (cpu_68k_family(g_sys_desc->cpu_subtype) == CPU_FAMILY_68060) ? VP_FLAG_HAS_BC : 0;

    self->base_priority = SCHED_PRI_FROM_QOS(VCPU_QOS_BACKGROUND, VCPU_PRI_LOWEST + 1);
    vcpu_on_sched_param_changed_np(self);
}

errno_t vcpu_create(vcpu_t _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    vcpu_t vp = NULL;

    err = kalloc(sizeof(struct vcpu), (void**) &vp);
    if (err == EOK) {
        vcpu_init(vp);
    }

    *pOutSelf = vp;
    return err;
}

void vcpu_destroy(vcpu_t _Nullable self)
{
    if (self) {
        assert(self->run_state == VCPU_STATE_SUSPENDED);

        stk_destroy(&self->kernel_stack);
        // Note: user stack is owned by the process
        kfree(self);
    }
}

void vcpu_reset_np(vcpu_t _Nonnull self, const vcpu_policy_t* _Nonnull policy, int nice, int quantum_boost)
{
    // First wipe out the old state and create a clean slate. We do this here
    // because doing this at relinquish time would be unsafe since the vcpu is
    // still running at the start of the relinquish phase
    self->dispatch_worker = NULL;
    self->proc = NULL;
    self->udata = 0;
    self->uerrno = 0;
    self->pending_sigs = 0;
    self->excpt_handler = (excpt_handler_t){0};
    self->excpt_state = (cpu_excpt_state_t){0};
    self->excpt_sa = NULL;
    self->syscall_sa = NULL;
    self->usr_ticks = 0;
    self->sys_ticks = 0;
    self->wait_ticks = 0;
    self->flags &= ~(VP_FLAG_DID_WAIT);


    // Setup QoS, nice, boosts
    _vcpu_reset_penalties_and_boosts_np(self);
    _vcpu_set_base_priority_np(self, policy);
    vcpu_set_nice_np(self, nice);
    vcpu_set_quantum_boost_np(self, quantum_boost);
    vcpu_on_sched_param_changed_np(self);
}

// @Entry Condition: preemption disabled
static void _vcpu_reset_penalties_and_boosts_np(vcpu_t _Nonnull self)
{
    self->priority_boost = 0;
    self->priority_penalty = 0;
    self->quantum_boost = 0;
    self->sched_nice = 0;
}

static int8_t _vcpu_effective_quantum_length_np(vcpu_t _Nonnull self)
{
    register const int8_t qos_class = SCHED_QOS_GRADE(self->cur_priority);
    register const int8_t base_len = g_quantum_base_length[qos_class];
    register const int8_t max_len = g_quantum_max_length[qos_class];

    return __min(base_len + self->quantum_boost, max_len);
}

// @Entry Condition: preemption disabled
void vcpu_reset_quantum_np(vcpu_t _Nonnull self)
{
    self->quantum_countdown = _vcpu_effective_quantum_length_np(self);
}

// @Entry Condition: preemption disabled
void vcpu_set_quantum_boost_np(vcpu_t _Nonnull self, int boost)
{
    self->quantum_boost = VCPU_CLAMPED_QUANTUM_BOOST(boost);
}

// @Entry Condition: preemption disabled
void vcpu_set_nice_np(vcpu_t _Nonnull self, int nice)
{
    const int new_nice = VCPU_CLAMPED_NICE_PRIORITY(nice);

    self->sched_nice = new_nice;
    if (self->sched_nice > 0) {
        // we give this guy a second chance
        self->priority_penalty = 0;
    }
}

// @Entry Condition: preemption disabled
static void _vcpu_set_base_priority_np(vcpu_t _Nonnull self, const vcpu_policy_t* _Nonnull policy)
{
    self->base_priority = SCHED_PRI_FROM_QOS(policy->qos.grade, policy->qos.priority);
}

// @Entry Condition: preemption disabled
void vcpu_on_sched_param_changed_np(vcpu_t _Nonnull self)
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
    self->quantum_countdown = _vcpu_effective_quantum_length_np(self);

    //XXX Note that we should not fully reset the quantum countdown here because
    // this effectively gives the vcpu a double-long quantum. Instead we should
    // calculate the fraction of the old quantum that is still unused and only
    // grant this fraction of the new quantum.
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

errno_t validate_vcpu_policy(const vcpu_policy_t* _Nonnull policy)
{
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

    return EOK;
}

errno_t vcpu_set_policy(vcpu_t _Nonnull self, const vcpu_policy_t* _Nonnull policy)
{
    decl_try_err();

    err = validate_vcpu_policy(policy);
    if (err != EOK) {
        return err;
    }


    const int sps = preempt_disable();
    switch (self->run_state) {
        case VCPU_STATE_READY:
            sched_set_unready(g_sched, self, false);
            _vcpu_set_base_priority_np(self, policy);
            vcpu_on_sched_param_changed_np(self);
            sched_set_ready(g_sched, self, true);
            break;
                
        case VCPU_STATE_RUNNING:
        case VCPU_STATE_WAITING:
        case VCPU_STATE_SUSPENDED:
            _vcpu_set_base_priority_np(self, policy);
            vcpu_on_sched_param_changed_np(self);
            break;

        default:
            abort();
    }
    preempt_restore(sps);

    return err;
}

// @Entry Condition: preemption disabled
static void vcpu_yield_np(vcpu_t _Nonnull self)
{
    if (self->run_state == VCPU_STATE_RUNNING) {
        if (!vcpu_is_fixed_pri(self) && self->priority_penalty > 0) {
            // Half the priority penalty, if any
            self->priority_penalty /= 2;
            vcpu_on_sched_param_changed_np(self);
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

    vcpu_yield_np(self);
    preempt_restore(sps);
}

errno_t vcpu_suspend(vcpu_t _Nonnull self)
{
    decl_try_err();
    const int sps = preempt_disable();
    vcpu_t cur_vp = vcpu_current();

    if (self == g_sched->idle_vp || self == g_sched->boot_vp) {
        throw(EINVAL);
    }
    if (!vcpu_is_user(self) && self != cur_vp) {
        // no involuntary suspension of kernel owned VPs
        throw(EPERM);
    }
    if (self->suspension_count == INT16_MAX) {
        throw(EINVAL);
    }

    self->suspension_count++;

    if (cur_vp == self) {
        // 'self' is currently running. Move it to suspended state immediately
        self->run_state = VCPU_STATE_SUSPENDED;
        sched_switch_to(g_sched, sched_highest_priority_ready(g_sched));
    }
    else if (self->suspension_count == 1 && self->run_state != VCPU_STATE_SUSPENDED) {
        // 'self' is some other vcpu in some state (running, ready, waiting). Trigger a deferred suspend on it
        vcpu_send_signal(self, SIG_URGENT);
    }

catch:
    preempt_restore(sps);

    return err;
}

void _vcpu_do_deferred_suspend_np(vcpu_t _Nonnull self)
{
    if (self->suspension_count > 0 && self->run_state != VCPU_STATE_SUSPENDED) {
        self->run_state = VCPU_STATE_SUSPENDED;
        sched_switch_to(g_sched, sched_highest_priority_ready(g_sched));
    }
}

void vcpu_resume(vcpu_t _Nonnull self, bool force)
{
    const int sps = preempt_disable();

    if (self->suspension_count > 0) {
        if (force) {
            self->suspension_count = 0;
        }
        else {
            self->suspension_count--;
        }


        if (self->suspension_count == 0 && self->run_state == VCPU_STATE_SUSPENDED) {
            sched_set_ready(g_sched, self, true);
        }
    }

    preempt_restore(sps);
}

// Checks whether 'self' is in the process of being suspended or is suspended,
// waits for suspension to have completed and returns EOK. Returns EBUSY if
// 'self' is not in process suspension and not suspended either. 
// @Entry Condition: preemption disabled
static errno_t vcpu_await_suspension_np(vcpu_t _Nonnull self)
{
    while (self->suspension_count > 0) {
        if (self->run_state == VCPU_STATE_SUSPENDED || self->run_state == VCPU_STATE_WAITING) {
            return EOK;
        }

        vcpu_yield_np(vcpu_current());
    }

    return EBUSY;
}

errno_t vcpu_await_suspension(vcpu_t _Nonnull self)
{
    const int sps = preempt_disable();
    const errno_t err = vcpu_await_suspension_np(self);
    preempt_restore(sps);

    return err;
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
            return EINVAL;
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
        err = vcpu_await_suspension_np(self);
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
            return EINVAL;
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
        err = vcpu_await_suspension_np(self);
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

            if (vcpu_is_user(self)) {
                ip->base_ptr = self->user_stack.base;
                ip->size = self->user_stack.size;
            }
            else {
                err = EINVAL;
            }
            break;
        }

        case VCPU_INFO_SCHEDULING: {
            vcpu_scheduling_info_t* ip = info;

            const int sps = preempt_disable();
            // Return VCPU_STATE_SUSPENDED if the suspended count is > 0 because
            // the true vcpu state can be any of suspended, waiting or even running
            // while the suspension count > 0. I.e. You call vcpu_suspend() on a
            // vcpu but it takes a while for it to reach a safe suspension point
            // (end of syscall handler). It'll remain in running state in the
            // meanwhile. Nevertheless the vcpu is suspended as far as user space
            // can tell.
            ip->run_state = (self->suspension_count > 0) ? VCPU_STATE_SUSPENDED : self->run_state;
            ip->suspend_count = self->suspension_count;
            ip->base_qos.grade = SCHED_QOS_GRADE(self->base_priority);
            ip->base_qos.priority = SCHED_QOS_PRI(self->base_priority);
            ip->cur_qos.grade = SCHED_QOS_GRADE(self->cur_priority);
            ip->cur_qos.priority = SCHED_QOS_PRI(self->cur_priority);
            ip->base_quantum_length = g_quantum_base_length[ip->base_qos.grade];
            ip->cur_quantum_length = _vcpu_effective_quantum_length_np(self);
            ip->flags = 0;
                
            if (self->priority_boost > 0)   ip->flags |= VCPU_HAS_PRIORITY_BOOST;
            if (self->quantum_boost > 0)    ip->flags |= VCPU_HAS_QUANTUM_BOOST;
            if (self->priority_penalty > 0) ip->flags |= VCPU_HAS_PRIORITY_PENALTY;
            preempt_restore(sps);
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

            clock_ticks2time(g_mono_clock, self->usr_ticks, &ip->user_time);
            clock_ticks2time(g_mono_clock, self->sys_ticks, &ip->system_time);
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

errno_t vcpu_set_excpt_handler(vcpu_t _Nonnull self, const excpt_handler_t* _Nullable handler, excpt_handler_t* _Nullable old_handler)
{
    if (handler->func == NULL) {
        return EINVAL;
    }

    if (old_handler) {
        *old_handler = self->excpt_handler;
    }
    self->excpt_handler = *handler;

    return EOK;
}

vcpuid_t new_vcpu_groupid(void)
{
    static atomic_int id = VCPUID_MAIN_GROUP;

    return (vcpuid_t)atomic_int_fetch_add(&id, 1);
}
