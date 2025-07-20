//
//  waitqueue.c
//  kernel
//
//  Created by Dietmar Planitzer on 6/28/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "waitqueue.h"
#include "vcpu.h"
#include <machine/csw.h>
#include <machine/MonotonicClock.h>


void wq_init(waitqueue_t _Nonnull self)
{
    List_Init(&self->q);
}

errno_t wq_deinit(waitqueue_t _Nonnull self)
{
    decl_try_err();
    const int sps = csw_disable();

    if (List_IsEmpty(&self->q)) {
        List_Deinit(&self->q);
    }
    else {
        err = EBUSY;
    }

    csw_restore(sps);
    return err;
}


// @Entry Condition: preemption disabled
// @Entry Condition: 'vp' must be in running state
static wres_t _one_shot_wait(waitqueue_t _Nonnull self, const sigset_t* _Nullable mask)
{
    sched_t ps = g_sched;
    vcpu_t vp = (vcpu_t)ps->running;
    const sigset_t oldMask = vp->sigmask;
    const sigset_t theMask = (mask) ? *mask : oldMask;
    const sigset_t availSigs = vp->psigs & ~theMask;

    assert(vp->sched_state == SCHED_STATE_RUNNING);


    if (availSigs) {
        return WRES_SIGNAL;
    }
    vp->sigmask = theMask;


    // FIFO order.
    List_InsertAfterLast(&self->q, &vp->rewa_qe);
    
    vp->sched_state = SCHED_STATE_WAITING;
    vp->waiting_on_wait_queue = self;
    vp->wait_start_time = MonotonicClock_GetCurrentQuantums(gMonotonicClock);
    vp->wakeup_reason = 0;

    
    // Find another VP to run and context switch to it
    sched_switch_to(ps,
            sched_highest_priority_ready(ps));
    
    vp->sigmask = oldMask;
    return vp->wakeup_reason;
}

// @Entry Condition: preemption disabled
static errno_t _one_shot_timedwait(waitqueue_t _Nonnull self, const sigset_t* _Nullable mask, int flags, const struct timespec* _Nullable wtp, struct timespec* _Nullable rmtp)
{
    sched_t ps = g_sched;
    vcpu_t vp = (vcpu_t)ps->running;
    struct timespec now, deadline;
    bool hasArmedTimer = false;

    // Put us on the timeout queue if a relevant timeout has been specified.
    // Note that we return immediately if we're already past the deadline
    MonotonicClock_GetCurrentTime(gMonotonicClock, &now);

    if ((flags & WAIT_ABSTIME) == WAIT_ABSTIME) {
        deadline = *wtp;
    }
    else {
        timespec_add(&now, wtp, &deadline);
    }


    if (timespec_lt(&deadline, &TIMESPEC_INF)) {
        if (timespec_le(&deadline, &now)) {
            if (rmtp) {
                timespec_clear(rmtp);
            }
            return WRES_TIMEOUT;
        }

        sched_arm_timeout(ps, vp, &deadline);
        hasArmedTimer = true;
    }


    // Now wait
    const wres_t res = _one_shot_wait(self, mask);
    if (hasArmedTimer) {
        sched_cancel_timeout(ps, vp);
    }


    // Calculate the unslept time, if requested
    if (rmtp) {
        MonotonicClock_GetCurrentTime(gMonotonicClock, &now);

        if (timespec_lt(&now, &deadline)) {
            timespec_sub(&deadline, &now, rmtp);
        }
        else {
            timespec_clear(rmtp);
        }
    }

    return res;
}

static int _best_pending_sig(vcpu_t _Nonnull vp, const sigset_t* _Nonnull set)
{
    const sigset_t avail_sigs = vp->psigs & *set;

    if (avail_sigs) {
        for (int i = 0; i < SIGMAX; i++) {
            const sigset_t sigbit = avail_sigs & (1 << i);
            
            if (sigbit) {
                return i + 1;
            }
        }
    }

    return 0;
}



// @Entry Condition: preemption disabled
errno_t wq_wait(waitqueue_t _Nonnull self, const sigset_t* _Nullable mask)
{
    switch (_one_shot_wait(self, mask)) {
        case WRES_WAKEUP:   return EOK;
        default:            return EINTR;
    }
}

// @Entry Condition: preemption disabled
errno_t wq_sigwait(waitqueue_t _Nonnull self, const sigset_t* _Nonnull set, siginfo_t* _Nullable info)
{
    sched_t ps = g_sched;
    vcpu_t vp = (vcpu_t)ps->running;
    const sigset_t mask = vp->sigmask & ~(*set);    // temporarily unblock signals in 'set'

    for (;;) {
        if (_one_shot_wait(self, &mask) == WRES_SIGNAL) {
            if (info) {
                const int signo = _best_pending_sig(vp, set);

                if (signo) {
                    vp->psigs &= ~_SIGBIT(signo);
                    info->signo = signo;
                    return EOK;
                }
            }

            return EINTR;
        }
    }

    /* NOT REACHED */
}

// @Entry Condition: preemption disabled
errno_t wq_timedwait(waitqueue_t _Nonnull self, const sigset_t* _Nullable mask, int flags, const struct timespec* _Nullable wtp, struct timespec* _Nullable rmtp)
{
    switch (_one_shot_timedwait(self, mask, flags, wtp, rmtp)) {
        case WRES_SIGNAL:   return EINTR;
        case WRES_TIMEOUT:  return ETIMEDOUT;
        default:            return EOK;
    }
}

// @Entry Condition: preemption disabled
errno_t wq_sigtimedwait(waitqueue_t _Nonnull self, const sigset_t* _Nonnull set, int flags, const struct timespec* _Nonnull wtp, siginfo_t* _Nullable info)
{
    sched_t ps = g_sched;
    vcpu_t vp = (vcpu_t)ps->running;
    const sigset_t mask = vp->sigmask & ~(*set);    // temporarily unblock signals in 'set'
    struct timespec now, deadline;
    
    // Convert a relative timeout to an absolute timeout because it makes it
    // easier to deal with spurious wakeups and we won't accumulate math errors
    // caused by time resolution limitations.
    if ((flags & WAIT_ABSTIME) == WAIT_ABSTIME) {
        deadline = *wtp;
    }
    else {
        MonotonicClock_GetCurrentTime(gMonotonicClock, &now);
        timespec_add(&now, wtp, &deadline);
    }


    for (;;) {
        switch (_one_shot_timedwait(self, &mask, flags, &deadline, NULL)) {
            case WRES_WAKEUP:   // Spurious wakeup
                break;

            case WRES_SIGNAL:
                if (info) {
                    const int signo = _best_pending_sig(vp, set);

                    if (signo) {
                        vp->psigs &= ~_SIGBIT(signo);
                        info->signo = signo;
                        return EOK;
                    }
                }
                return EINTR;

            case WRES_TIMEOUT:
                return ETIMEDOUT;
        }
    }

    /* NOT REACHED */
}


// @Interrupt Context: Safe
// @Entry Condition: preemption disabled
bool wq_wakeone(waitqueue_t _Nonnull self, vcpu_t _Nonnull vp, int flags, wres_t reason)
{
    sched_t ps = g_sched;
    bool isReady;


    // Nothing to do if we are not waiting
    if (vp->sched_state != SCHED_STATE_WAITING) {
        return false;
    }
    

    // Finish the wait. Remove the VP from the wait queue, the timeout queue and
    // store the wake reason.
    List_Remove(&self->q, &vp->rewa_qe);
    sched_cancel_timeout(ps, vp);
    
    vp->waiting_on_wait_queue = NULL;
    vp->wakeup_reason = reason;
    

    if (vp->suspension_count == 0) {
        // Make the VP ready and adjust it's effective priority based on the
        // time it has spent waiting
        const int32_t quartersSlept = (MonotonicClock_GetCurrentQuantums(gMonotonicClock) - vp->wait_start_time) / ps->quantums_per_quarter_second;
        const int8_t boostedPriority = __min(vp->effectivePriority + __min(quartersSlept, VP_PRIORITY_HIGHEST), VP_PRIORITY_HIGHEST);
        sched_add_vcpu_locked(ps, vp, boostedPriority);
        
        if ((flags & WAKEUP_CSW) == WAKEUP_CSW) {
            sched_maybe_switch_to(ps, vp);
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
    sched_suspend_timeout(g_sched, vp);
}

// @Entry Condition: preemption disabled
void wq_resumeone(waitqueue_t _Nonnull self, vcpu_t _Nonnull vp)
{
    // Still in waiting state -> just resume the timeout if one is
    // associated with the wait.
    sched_resume_timeout(g_sched, vp, vp->suspension_time);
}
