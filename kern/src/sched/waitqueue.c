//
//  waitqueue.c
//  kernel
//
//  Created by Dietmar Planitzer on 6/28/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "waitqueue.h"
#include "vcpu.h"
#include <assert.h>
#include <ext/math.h>
#include <hal/clock.h>
#include <hal/sched.h>
#include <process/ProcessPriv.h>

#define WRES_WAKEUP     1
#define WRES_TIMEOUT    2


void wq_init(waitqueue_t _Nonnull self)
{
    self->q = DEQUE_INIT;
}

errno_t wq_deinit(waitqueue_t _Nonnull self)
{
    decl_try_err();
    const int sps = preempt_disable();

    if (!deque_empty(&self->q)) {
        err = EBUSY;
    }

    preempt_restore(sps);
    return err;
}

// 
// @Entry Condition: preemption disabled
// @Entry Condition: 'vp' must be in running state
void wq_wait_np(waitqueue_t _Nonnull self)
{
    vcpu_t vp = (vcpu_t)g_sched->running;
    const ticks_t start_ticks = clock_getticks(g_mono_clock);

    assert(vp->run_state == VCPU_STATE_RUNNING);

    // FIFO order.
    deque_add_last(&self->q, &vp->rewa_qe);
    
    vp->run_state = VCPU_STATE_WAITING;
    vp->waiting_on_wait_queue = self;
    vp->wakeup_reason = 0;
    vp->flags |= VP_FLAG_DID_WAIT;
    if (vp->quantum_countdown > 0) {
        // Reduce the remaining quantum size a bit to ensure that this vcpu
        // doesn't end up monopolizing the CPU after wakeup just because it
        // frequently enters the wait state and thus the quantum doesn't get
        // much of a chance to "run down" the natural way.
        vp->quantum_countdown -= SCHED_QUANTUM_NUDGE;
    }


    // Find another VP to run and context switch to it
    vp->proc->vcpu_waiting_count++;
    sched_switch_to(g_sched, sched_highest_priority_ready(g_sched));
    vp->proc->vcpu_waiting_count--;

    
    // Update the wait time stats
    const ticks_t stop_ticks = clock_getticks(g_mono_clock);
    const ticks_t wait_ticks = stop_ticks - start_ticks;
    vp->wait_ticks += wait_ticks;
    vp->proc->wait_ticks += wait_ticks;
}

// 
// @Entry Condition: preemption disabled
// @Entry Condition: 'vp' must be in running state
bool wq_timedwait_np(waitqueue_t _Nonnull self, int flags, const nanotime_t* _Nonnull wtp)
{
    vcpu_t vp = (vcpu_t)g_sched->running;
    const ticks_t start_ticks = clock_getticks(g_mono_clock);
    ticks_t deadline_ticks;
    bool armTimeout = false;

    assert(vp->run_state == VCPU_STATE_RUNNING);


    // Put us on the timeout queue. Note that we return immediately if we're
    // already past the deadline
    if (nanotime_lt(wtp, &NANOTIME_INF)) {
        deadline_ticks = clock_time2ticks_ceil(g_mono_clock, wtp);

        if ((flags & WAIT_ABSTIME) == 0) {
            deadline_ticks += start_ticks; 
        }

        if (deadline_ticks <= start_ticks) {
            return true;
        }

        armTimeout = true;
    }


    // FIFO order.
    deque_add_last(&self->q, &vp->rewa_qe);
    
    vp->run_state = VCPU_STATE_WAITING;
    vp->waiting_on_wait_queue = self;
    vp->wakeup_reason = 0;
    vp->flags |= VP_FLAG_DID_WAIT;
    if (vp->quantum_countdown > 0) {
        // Reduce the remaining quantum size a bit to ensure that this vcpu
        // doesn't end up monopolizing the CPU after wakeup just because it
        // frequently enters the wait state and thus the quantum doesn't get
        // much of a chance to "run down" the natural way.
        vp->quantum_countdown -= SCHED_QUANTUM_NUDGE;
    }

    
    if (armTimeout) {
        vp->timeout.deadline = deadline_ticks;
        vp->timeout.func = (deadline_func_t)sched_wait_timeout_irq;
        vp->timeout.arg = vp;

        clock_deadline(g_mono_clock, &vp->timeout);
    }


    // Find another VP to run and context switch to it
    vp->proc->vcpu_waiting_count++;
    sched_switch_to(g_sched, sched_highest_priority_ready(g_sched));
    vp->proc->vcpu_waiting_count--;


    if (armTimeout) {
        clock_cancel_deadline(g_mono_clock, &vp->timeout);
    }

    
    // Update the wait time stats
    const ticks_t stop_ticks = clock_getticks(g_mono_clock);
    const ticks_t wait_ticks = stop_ticks - start_ticks;
    vp->wait_ticks += wait_ticks;
    vp->proc->wait_ticks += wait_ticks;

    return false;
}

// @Interrupt Context: Safe
// @Entry Condition: preemption disabled
void wq_wakeup_vcpu_np(waitqueue_t _Nonnull self, vcpu_t _Nonnull vp, int flags, int pri_boost)
{
    // Nothing to do if we are not waiting
    if (vp->run_state != VCPU_STATE_WAITING) {
        return;
    }
    

    // Remove the VP from the wait queue
    deque_remove(&self->q, &vp->rewa_qe);
    clock_cancel_deadline(g_mono_clock, &vp->timeout);

    vp->waiting_on_wait_queue = NULL;
    vp->wakeup_reason = ((flags & WAKEUP_TIMEOUT) == WAKEUP_TIMEOUT) ? WRES_TIMEOUT : WRES_WAKEUP;


    if (!vcpu_is_fixed_pri(vp)) {
        bool do_sched_params_changed = false;

        // Reset the scheduling penalty if one exists
        if (vp->priority_penalty > 0) {
            vp->priority_penalty = 0;
            do_sched_params_changed = true;
        }

        // Apply the priority boost. Note that the new boost replaces an already
        // existing boost. Boost values do not stack.
        if (pri_boost > 0) {
            vp->priority_boost = __min(pri_boost, VCPU_PRI_COUNT-1);
            do_sched_params_changed = true;
        }

        if (do_sched_params_changed) {
            vcpu_on_sched_param_changed(vp);
        }
    }


    // Restore the quantum for some QoS classes
    if (vcpu_effective_qos_class(vp) == VCPU_QOS_REALTIME) {
        vcpu_reset_quantum(vp);
    }


    // Make the VP ready and move it to the front of its ready queue if it
    // didn't use all of its quantum before blocking
    sched_set_ready(g_sched, vp, true);
    

    // Context switch immediately to the receiver is allowed to do so, we're not
    // running in the interrupt context and the waiting vcpu priority is higher
    // or the same as ours.
    if (((flags & WAKEUP_NO_IMMED_CSW) == 0) && !sched_is_irq_ctx(g_sched)) {
        if (vcpu_effective_qos_class(vp) >= VCPU_QOS_INTERACTIVE) {
            vcpu_t pBestReadyVP = sched_highest_priority_ready(g_sched);
    
            if (pBestReadyVP == vp && vp->cur_priority >= g_sched->running->cur_priority) {
                sched_switch_to(g_sched, vp);
            }
        }
    }
}

// Wakes up either one or all waiters on the wait queue. The woken up VPs are
// removed from the wait queue. Expects to be called with preemption disabled.
// @Entry Condition: preemption disabled
void wq_wakeup_many_np(waitqueue_t _Nonnull self, int flags, int pri_boost)
{
    register deque_node_t* cp = self->q.first;

    if ((flags & WAKEUP_ONE) == 0) {
        // Make all waiting VPs ready and find a VP to potentially context switch to.
        while (cp) {
            register deque_node_t* np = cp->next;
            
            wq_wakeup_vcpu_np(self, (vcpu_t)cp, WAKEUP_NO_IMMED_CSW, pri_boost);
            cp = np;
        }
    }
    else {
        // Wake the first waiter
        wq_wakeup_vcpu_np(self, (vcpu_t)cp, flags & ~WAKEUP_ONE, pri_boost);
    }
}
