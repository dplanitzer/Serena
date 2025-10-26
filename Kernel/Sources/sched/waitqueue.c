//
//  waitqueue.c
//  kernel
//
//  Created by Dietmar Planitzer on 6/28/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "waitqueue.h"
#include "vcpu.h"
#include <machine/clock.h>
#include <machine/csw.h>


void wq_init(waitqueue_t _Nonnull self)
{
    self->q = LIST_INIT;
}

errno_t wq_deinit(waitqueue_t _Nonnull self)
{
    decl_try_err();
    const int sps = preempt_disable();

    if (!List_IsEmpty(&self->q)) {
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

    assert(vp->sched_state == SCHED_STATE_RUNNING);


    if ((vp->pending_sigs & hot_sigs) != 0) {
        return WRES_SIGNAL;
    }


    // FIFO order.
    List_InsertAfterLast(&self->q, &vp->rewa_qe);
    
    vp->sched_state = SCHED_STATE_WAITING;
    vp->waiting_on_wait_queue = self;
    vp->wait_sigs = hot_sigs;
    vp->wakeup_reason = 0;

    

    if (armTimeout) {
        clock_deadline(g_mono_clock, &vp->timeout);
    }

    // Find another VP to run and context switch to it
    sched_switch_to(g_sched, sched_highest_priority_ready(g_sched), false);
    
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
bool wq_wakeone(waitqueue_t _Nonnull self, vcpu_t _Nonnull vp, int flags, wres_t reason)
{
    bool isReady;


    // Nothing to do if we are not waiting
    if (vp->sched_state != SCHED_STATE_WAITING) {
        return false;
    }
    

    // Finish the wait. Remove the VP from the wait queue, the timeout queue and
    // store the wake reason.
    List_Remove(&self->q, &vp->rewa_qe);
    clock_cancel_deadline(g_mono_clock, &vp->timeout);
    
    vp->waiting_on_wait_queue = NULL;
    vp->wakeup_reason = reason;
    

    if (vp->suspension_count == 0) {
        // Make the VP ready and move it to the front of its ready queue if it
        // didn't use all of its quantum before blocking
        sched_set_ready(g_sched, vp, vp->sched_priority, (vp->quantum_countdown >= 1) ? false : true);
        
        if ((flags & WAKEUP_CSW) == WAKEUP_CSW) {
            sched_maybe_switch_to(g_sched, vp);
        }
        isReady = true;
    } else {
        // The VP is suspended. Move it to ready state so that it will be
        // added to the ready queue once we resume it.
        vp->sched_state = SCHED_STATE_READY;
        isReady = false;
    }

    return isReady;
}

// Wakes up either one or all waiters on the wait queue. The woken up VPs are
// removed from the wait queue. Expects to be called with preemption disabled.
// @Entry Condition: preemption disabled
void wq_wake(waitqueue_t _Nonnull self, int flags, wres_t reason)
{
    register ListNode* cp = self->q.first;
    register bool isWakeupOne = ((flags & WAKEUP_ONE) == WAKEUP_ONE);
    vcpu_t pRunCandidate = NULL;

    
    // Make all waiting VPs ready and find a VP to potentially context switch to.
    while (cp) {
        register ListNode* np = cp->next;
        register vcpu_t vp = (vcpu_t)cp;
        register const bool isReady = wq_wakeone(self, vp, 0, reason);

        if (pRunCandidate == NULL && isReady) {
            pRunCandidate = vp;
        }
        if (isWakeupOne) {
            break;
        }

        cp = np;
    }
        
    
    // Set the VP that we found running if context switches are allowed.
    if ((flags & WAKEUP_CSW) == WAKEUP_CSW && pRunCandidate) {
        sched_maybe_switch_to(g_sched, pRunCandidate);
    }
}

// Adds all VPs on the given list to the ready queue. The VPs are removed from
// the wait queue. Expects to be called from an interrupt context and thus defers
// context switches until the return from the interrupt context.
// @Entry Condition: preemption disabled
void wq_wake_irq(waitqueue_t _Nonnull self)
{
    register ListNode* cp = self->q.first;    
    
    // Make all waiting VPs ready to run but do not trigger a context switch.
    while (cp) {
        register ListNode* np = cp->next;
        
        wq_wakeone(self, (vcpu_t)cp, 0, WRES_WAKEUP);
        cp = np;
    }
}


// @Entry Condition: preemption disabled
void wq_suspendone(waitqueue_t _Nonnull self, vcpu_t _Nonnull vp)
{
    // We do not interrupt the wait because we'll just treat it as
    // a longer-than-expected wait. However we suspend the timeout
    // while the VP is suspended. The resume will reactive the
    // timeout and extend it by the amount of time that the VP has
    // spent in suspended state.
    if (clock_cancel_deadline(g_mono_clock, &vp->timeout)) {
        vp->flags |= VP_FLAG_TIMEOUT_SUSPENDED;
    }
}

// @Entry Condition: preemption disabled
void wq_resumeone(waitqueue_t _Nonnull self, vcpu_t _Nonnull vp)
{
    // Still in waiting state -> just resume the timeout if one is
    // associated with the wait.
    if ((vp->flags & VP_FLAG_TIMEOUT_SUSPENDED) == VP_FLAG_TIMEOUT_SUSPENDED) {
        vp->flags &= ~VP_FLAG_TIMEOUT_SUSPENDED;
        vp->timeout.deadline += __max(clock_getticks(g_mono_clock) - vp->suspension_time, 0);
        clock_deadline(g_mono_clock, &vp->timeout);
    }
}
