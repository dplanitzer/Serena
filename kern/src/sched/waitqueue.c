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


// The basic non-time-limited wait primitive. This function waits on the wait
// queue until it is explicitly woken up by one of the wake() calls or a signal
// arrives that is in the signal set 'set'. Note that 'set' is accepted as is
// and this function does _not_ ensure that non-maskable signals are added to
// 'set'. It's your responsibility to do this if so desired. Enables just
// non-maskable signals if 'set' is NULL.
// @Entry Condition: preemption disabled
// @Entry Condition: 'vp' must be in running state
wres_t wq_prim_wait(waitqueue_t _Nonnull self, const sigset_t* _Nullable set, bool armTimeout)
{
    vcpu_t vp = (vcpu_t)g_sched->running;
    const sigset_t hot_sigs = (set) ? *set : SIGSET_NONMASKABLES;

    assert(vp->run_state == VCPU_RUST_RUNNING);


    if ((vp->pending_sigs & hot_sigs) != 0) {
        return WRES_SIGNAL;
    }


    // FIFO order.
    deque_add_last(&self->q, &vp->rewa_qe);
    
    vp->run_state = VCPU_RUST_WAITING;
    vp->waiting_on_wait_queue = self;
    vp->wait_sigs = hot_sigs;
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
        clock_deadline(g_mono_clock, &vp->timeout);
    }

    // Find another VP to run and context switch to it
    sched_switch_to(g_sched, sched_highest_priority_ready(g_sched));
    
    if (armTimeout) {
        clock_cancel_deadline(g_mono_clock, &vp->timeout);
    }


    return vp->wakeup_reason;
}

// Same as wq_prim_wait() but cancels the wait once the wait deadline specified
// by 'wtp' has arrived.
// @Entry Condition: preemption disabled
wres_t wq_prim_timedwait(waitqueue_t _Nonnull self, const sigset_t* _Nullable mask, int flags, const struct timespec* _Nullable wtp, struct timespec* _Nullable rmtp)
{
    vcpu_t vp = (vcpu_t)g_sched->running;
    tick_t now, deadline;
    bool armTimeout = false;

    // Put us on the timeout queue if a relevant timeout has been specified.
    // Note that we return immediately if we're already past the deadline
    if (wtp && timespec_lt(wtp, &TIMESPEC_INF)) {
        now = clock_getticks(g_mono_clock);
        deadline = clock_time2ticks_ceil(g_mono_clock, wtp);

        if ((flags & WAIT_ABSTIME) == 0) {
            deadline += now; 
        }

        if (deadline <= now) {
            if (rmtp) {
                timespec_clear(rmtp);
            }
            return WRES_TIMEOUT;
        }


        vp->timeout.deadline = deadline;
        vp->timeout.func = (deadline_func_t)sched_wait_timeout_irq;
        vp->timeout.arg = vp;

        armTimeout = true;
    }


    // Now wait
    const wres_t res = wq_prim_wait(self, mask, armTimeout);


    // Calculate the unslept time, if requested
    if (wtp && rmtp) {
        now = clock_getticks(g_mono_clock);

        if (now < deadline) {
            clock_ticks2time(g_mono_clock, deadline - now, rmtp);
        }
        else {
            timespec_clear(rmtp);
        }
    }
    else if (rmtp) {
        timespec_clear(rmtp);
    } 

    return res;
}



// @Entry Condition: preemption disabled
errno_t wq_wait(waitqueue_t _Nonnull self, const sigset_t* _Nullable set)
{
    switch (wq_prim_wait(self, set, false)) {
        case WRES_WAKEUP:   return EOK;
        default:            return EINTR;
    }
}

// @Entry Condition: preemption disabled
errno_t wq_timedwait(waitqueue_t _Nonnull self, const sigset_t* _Nullable set, int flags, const struct timespec* _Nullable wtp, struct timespec* _Nullable rmtp)
{
    switch (wq_prim_timedwait(self, set, flags, wtp, rmtp)) {
        case WRES_SIGNAL:   return EINTR;
        case WRES_TIMEOUT:  return ETIMEDOUT;
        default:            return EOK;
    }
}


// @Interrupt Context: Safe
// @Entry Condition: preemption disabled
void wq_wakeup_vcpu(waitqueue_t _Nonnull self, vcpu_t _Nonnull vp, int flags, wres_t reason, int pri_boost)
{
    // Nothing to do if we are not waiting
    if (vp->run_state != VCPU_RUST_WAITING) {
        return;
    }
    

    // Finish the wait. Remove the VP from the wait queue, the timeout queue and
    // store the wake reason.
    deque_remove(&self->q, &vp->rewa_qe);
    clock_cancel_deadline(g_mono_clock, &vp->timeout);
    
    vp->waiting_on_wait_queue = NULL;
    vp->wakeup_reason = reason;
    

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
void wq_wakeup_many(waitqueue_t _Nonnull self, int flags, wres_t reason, int pri_boost)
{
    register deque_node_t* cp = self->q.first;

    if ((flags & WAKEUP_ONE) == 0) {
        // Make all waiting VPs ready and find a VP to potentially context switch to.
        while (cp) {
            register deque_node_t* np = cp->next;
            
            wq_wakeup_vcpu(self, (vcpu_t)cp, WAKEUP_NO_IMMED_CSW, reason, pri_boost);
            cp = np;
        }
    }
    else {
        // Wake the first waiter
        wq_wakeup_vcpu(self, (vcpu_t)cp, flags & ~WAKEUP_ONE, reason, pri_boost);
    }
}
