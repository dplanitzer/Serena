//
//  WaitQueue.c
//  kernel
//
//  Created by Dietmar Planitzer on 6/28/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "WaitQueue.h"
#include <hal/MonotonicClock.h>
#include <hal/Platform.h>
#include "VirtualProcessorScheduler.h"


const sigset_t SIGSET_BLOCK_ALL = UINT32_MAX;
const sigset_t SIGSET_BLOCK_NONE = 0;


void WaitQueue_Init(WaitQueue* _Nonnull self)
{
    List_Init(&self->q);
}

errno_t WaitQueue_Deinit(WaitQueue* self)
{
    decl_try_err();
    const int sps = preempt_disable();

    if (List_IsEmpty(&self->q)) {
        List_Deinit(&self->q);
    }
    else {
        err = EBUSY;
    }

    preempt_restore(sps);
    return err;
}


// @Entry Condition: preemption disabled
// @Entry Condition: 'vp' must be in running state
static wres_t _one_shot_wait(WaitQueue* _Nonnull self, const sigset_t* _Nullable mask, sigset_t* _Nullable osigs)
{
    VirtualProcessorScheduler* ps = gVirtualProcessorScheduler;
    VirtualProcessor* vp = (VirtualProcessor*)ps->running;
    const sigset_t oldMask = vp->sigmask;
    const sigset_t theMask = (mask) ? *mask : oldMask;
    const sigset_t availSigs = vp->psigs & ~theMask;

    assert(vp->sched_state == SCHED_STATE_RUNNING);


    // Immediately return instead of waiting if we are in the middle of an abort
    // of a call-as-user invocation.
    if ((vp->flags & (VP_FLAG_CAU_IN_PROGRESS | VP_FLAG_CAU_ABORTED)) == (VP_FLAG_CAU_IN_PROGRESS | VP_FLAG_CAU_ABORTED)) {
        return WRES_SIGNAL;
    }

    if (availSigs) {
        if (osigs) {
            *osigs = availSigs;
            vp->psigs &= ~availSigs;
        }
        return WRES_SIGNAL;
    }
    vp->sigmask = theMask;


    // FIFO order.
    List_InsertAfterLast(&self->q, &vp->rewa_qe);
    
    vp->sched_state = SCHED_STATE_WAITING;
    vp->waiting_on_wait_queue = self;
    vp->wait_start_time = MonotonicClock_GetCurrentQuantums();
    vp->wakeup_reason = 0;

    
    // Find another VP to run and context switch to it
    VirtualProcessorScheduler_SwitchTo(ps,
            VirtualProcessorScheduler_GetHighestPriorityReady(ps));
    
    vp->sigmask = oldMask;
    return vp->wakeup_reason;
}

// @Entry Condition: preemption disabled
errno_t WaitQueue_Wait(WaitQueue* _Nonnull self, const sigset_t* _Nullable mask)
{
    switch (_one_shot_wait(self, mask, NULL)) {
        case WRES_WAKEUP:   return EOK;
        default:            return EINTR;
    }
}

errno_t WaitQueue_SigWait(WaitQueue* _Nonnull self, const sigset_t* _Nullable mask, sigset_t* _Nonnull osigs)
{
    const int sps = preempt_disable();
    while (_one_shot_wait(self, mask, osigs) != WRES_SIGNAL);
    preempt_restore(sps);

    return EOK;
}

// @Entry Condition: preemption disabled
errno_t WaitQueue_TimedWait(WaitQueue* _Nonnull self, const sigset_t* _Nullable mask, int flags, const struct timespec* _Nullable wtp, struct timespec* _Nullable rmtp)
{
    VirtualProcessorScheduler* ps = gVirtualProcessorScheduler;
    VirtualProcessor* vp = (VirtualProcessor*)ps->running;
    struct timespec now, deadline;
    bool hasArmedTimer = false;

    // Put us on the timeout queue if a relevant timeout has been specified.
    // Note that we return immediately if we're already past the deadline
    MonotonicClock_GetCurrentTime(&now);

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
            return ETIMEDOUT;
        }

        VirtualProcessorScheduler_ArmTimeout(ps, vp, &deadline);
        hasArmedTimer = true;
    }


    // Now wait
    const wres_t res = _one_shot_wait(self, mask, NULL);
    if (hasArmedTimer) {
        VirtualProcessorScheduler_CancelTimeout(ps, vp);
    }


    // Calculate the unslept time, if requested
    if (rmtp) {
        MonotonicClock_GetCurrentTime(&now);

        if (timespec_lt(&now, &deadline)) {
            timespec_sub(&deadline, &now, rmtp);
        }
        else {
            timespec_clear(rmtp);
        }
    }


    switch (res) {
        case WRES_SIGNAL:   return EINTR;
        case WRES_TIMEOUT:  return ETIMEDOUT;
        default:            return EOK;
    }
}

errno_t WaitQueue_SigTimedWait(WaitQueue* _Nonnull self, const sigset_t* _Nullable mask, sigset_t* _Nonnull osigs, int flags, const struct timespec* _Nonnull wtp)
{
    decl_try_err();
    struct timespec now, deadline;
    
    // Convert a relative timeout to an absolute timeout because it makes it
    // easier to deal with spurious wakeups and we won't accumulate math errors
    // caused by time resolution limitations.
    if ((flags & WAIT_ABSTIME) == WAIT_ABSTIME) {
        deadline = *wtp;
    }
    else {
        MonotonicClock_GetCurrentTime(&now);
        timespec_add(&now, wtp, &deadline);
    }


    const int sps = preempt_disable();
    for (;;) {
        err = WaitQueue_TimedWait(self, mask, flags, &deadline, NULL);
        if (err == EINTR) {
            err = EOK;
            break;
        }
        if (err == ETIMEDOUT) {
            break;
        }
    }
    preempt_restore(sps);

    return err;
}


// @Interrupt Context: Safe
// @Entry Condition: preemption disabled
bool WaitQueue_WakeupOne(WaitQueue* _Nonnull self, VirtualProcessor* _Nonnull vp, int flags, int signo)
{
    VirtualProcessorScheduler* ps = gVirtualProcessorScheduler;
    bool isReady;


    // Update the signal state if a real signal was provided
    if (signo >= SIGMIN && signo <= SIGMAX) {
        const sigset_t sigbit = 1 << (signo - 1);

        vp->psigs |= sigbit;
        if ((sigbit & ~vp->sigmask) == 0) {
            return false;
        }
    }


    // Nothing to do if we are not waiting
    if (vp->sched_state != SCHED_STATE_WAITING) {
        return false;
    }
    

    // Finish the wait. Remove the VP from the wait queue, the timeout queue and
    // store the wake reason.
    List_Remove(&self->q, &vp->rewa_qe);
    VirtualProcessorScheduler_CancelTimeout(ps, vp);
    
    vp->waiting_on_wait_queue = NULL;
    
    switch (signo) {
        case SIGNULL:       vp->wakeup_reason = WRES_WAKEUP; break;
        case SIGTIMEOUT:    vp->wakeup_reason = WRES_TIMEOUT; break;
        default:            vp->wakeup_reason = WRES_SIGNAL; break;
    }
    

    if (vp->suspension_count == 0) {
        // Make the VP ready and adjust it's effective priority based on the
        // time it has spent waiting
        const int32_t quartersSlept = (MonotonicClock_GetCurrentQuantums() - vp->wait_start_time) / ps->quantums_per_quarter_second;
        const int8_t boostedPriority = __min(vp->effectivePriority + __min(quartersSlept, VP_PRIORITY_HIGHEST), VP_PRIORITY_HIGHEST);
        VirtualProcessorScheduler_AddVirtualProcessor_Locked(ps, vp, boostedPriority);
        
        if ((flags & WAKEUP_CSW) == WAKEUP_CSW) {
            VirtualProcessorScheduler_MaybeSwitchTo(ps, vp);
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
void WaitQueue_Wakeup(WaitQueue* _Nonnull self, int flags, int signo)
{
    register ListNode* cp = self->q.first;
    register bool isWakeupOne = ((flags & WAKEUP_ONE) == WAKEUP_ONE);
    VirtualProcessor* pRunCandidate = NULL;

    
    // Make all waiting VPs ready and find a VP to potentially context switch to.
    while (cp) {
        register ListNode* np = cp->next;
        register VirtualProcessor* vp = (VirtualProcessor*)cp;
        register const bool isReady = WaitQueue_WakeupOne(self, vp, 0, signo);

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
        VirtualProcessorScheduler_MaybeSwitchTo(gVirtualProcessorScheduler, pRunCandidate);
    }
}

// Adds all VPs on the given list to the ready queue. The VPs are removed from
// the wait queue. Expects to be called from an interrupt context and thus defers
// context switches until the return from the interrupt context.
// @Entry Condition: preemption disabled
void WaitQueue_WakeupAllFromInterrupt(WaitQueue* _Nonnull self)
{
    register ListNode* cp = self->q.first;    
    
    // Make all waiting VPs ready to run but do not trigger a context switch.
    while (cp) {
        register ListNode* np = cp->next;
        
        WaitQueue_WakeupOne(self, (VirtualProcessor*)cp, 0, 0);
        cp = np;
    }
}


// @Entry Condition: preemption disabled
void WaitQueue_SuspendOne(WaitQueue* _Nonnull self, struct VirtualProcessor* _Nonnull vp)
{
    // We do not interrupt the wait because we'll just treat it as
    // a longer-than-expected wait. However we suspend the timeout
    // while the VP is suspended. The resume will reactive the
    // timeout and extend it by the amount of time that the VP has
    // spent in suspended state.
    VirtualProcessorScheduler_SuspendTimeout(gVirtualProcessorScheduler, vp);
}

// @Entry Condition: preemption disabled
void WaitQueue_ResumeOne(WaitQueue* _Nonnull self, struct VirtualProcessor* _Nonnull vp)
{
    // Still in waiting state -> just resume the timeout if one is
    // associated with the wait.
    VirtualProcessorScheduler_ResumeTimeout(gVirtualProcessorScheduler, vp, vp->suspension_time);
}
