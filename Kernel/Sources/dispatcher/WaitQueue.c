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
static errno_t _do_wait(WaitQueue* _Nonnull self, int flags, VirtualProcessorScheduler* _Nonnull ps, VirtualProcessor* _Nonnull vp)
{
    assert(vp->sched_state == kVirtualProcessorState_Running);


    // Immediately return instead of waiting if we are in the middle of an abort
    // of a call-as-user invocation.
    if ((vp->flags & (VP_FLAG_CAU_IN_PROGRESS | VP_FLAG_CAU_ABORTED)) == (VP_FLAG_CAU_IN_PROGRESS | VP_FLAG_CAU_ABORTED)) {
        return EINTR;
    }


    // FIFO order.
    List_InsertAfterLast(&self->q, &vp->rewa_queue_entry);
    
    vp->sched_state = kVirtualProcessorState_Waiting;
    vp->waiting_on_wait_queue = self;
    vp->wait_start_time = MonotonicClock_GetCurrentQuantums();
    vp->wakeup_reason = WAKEUP_REASON_NONE;

    if ((flags & WAIT_INTERRUPTABLE) == WAIT_INTERRUPTABLE) {
        vp->flags |= VP_FLAG_INTERRUPTABLE_WAIT;
    } else {
        vp->flags &= ~VP_FLAG_INTERRUPTABLE_WAIT;
    }

    
    // Find another VP to run and context switch to it
    VirtualProcessorScheduler_SwitchTo(ps,
            VirtualProcessorScheduler_GetHighestPriorityReady(ps));
    
    
    switch (vp->wakeup_reason) {
        case WAKEUP_REASON_INTERRUPTED: return EINTR;
        case WAKEUP_REASON_TIMEOUT:     return ETIMEDOUT;
        case WAKEUP_REASON_SIGNALLED:   return EOK;     // Note that the arrival of a signal is the desired outcome for a sigwait()/sigtimedwait(). Remember, this is not POSIX
        default:                        return EOK;
    }
}

errno_t WaitQueue_Wait(WaitQueue* _Nonnull self, int flags)
{
    VirtualProcessorScheduler* ps = gVirtualProcessorScheduler;
    VirtualProcessor* vp = (VirtualProcessor*)ps->running;

    return _do_wait(self, flags, ps, vp);
}

errno_t WaitQueue_TimedWait(WaitQueue* _Nonnull self, int flags, const struct timespec* _Nullable wtp, struct timespec* _Nullable rmtp)
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
    const errno_t err = _do_wait(self, flags, ps, vp);
    if (err != EOK && hasArmedTimer) {
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

    return err;
}

errno_t WaitQueue_SigWait(WaitQueue* _Nonnull self, int flags, unsigned int* _Nullable pOutSigs)
{
    decl_try_err();
    VirtualProcessorScheduler* ps = gVirtualProcessorScheduler;
    VirtualProcessor* vp = (VirtualProcessor*)ps->running;

    do {
        const uint32_t psigs = vp->psigs & vp->sigmask;

        if (psigs) {
            if (pOutSigs) {
                *pOutSigs = psigs;
            }

            vp->psigs &= ~vp->sigmask;
            break;
        }

        // Waiting temporarily reenables preemption
        err = _do_wait(self, flags, ps, vp);
    } while (err == EOK);

    return err;
}

errno_t WaitQueue_SigTimedWait(WaitQueue* _Nonnull self, int flags, const struct timespec* _Nullable wtp, struct timespec* _Nullable rmtp, unsigned int* _Nullable pOutSigs)
{
    decl_try_err();
    VirtualProcessorScheduler* ps = gVirtualProcessorScheduler;
    VirtualProcessor* vp = (VirtualProcessor*)ps->running;
    struct timespec now, abst, remt;
    
    // Convert a relative timeout to an absolute timeout because it makes it
    // easier to deal with spurious wakeups and we won't accumulate math errors
    // caused by time resolution limitations.
    if ((flags & WAIT_ABSTIME) == WAIT_ABSTIME) {
        abst = *wtp;
    }
    else {
        MonotonicClock_GetCurrentTime(&now);
        timespec_add(&now, wtp, &abst);
    }


    do {
        const uint32_t psigs = vp->psigs & vp->sigmask;

        if (psigs) {
            if (pOutSigs) {
                *pOutSigs = psigs;
            }

            vp->psigs &= ~vp->sigmask;
            break;
        }

        // Waiting temporarily reenables preemption
        err = WaitQueue_TimedWait(self, flags, &abst, &remt);
    } while (err == EOK);
    

    if (rmtp) {
        *rmtp = remt;
    }

    return err;
}


// Adds the given VP from the given wait queue to the ready queue. The VP is
// removed from the wait queue. The scheduler guarantees that a wakeup operation
// will never fail with an error. This doesn't mean that calling this function
// will always result in a virtual processor wakeup. If the wait queue is empty
// then no wakeups will happen. Also a virtual processor that sits in an
// uninterruptible wait or that was suspended while being in a wait state will
// not get woken up. Returns true if the VP has been made ready to run; false
// otherwise.
// May be called from an interrupt context.
bool WaitQueue_WakeupOne(WaitQueue* _Nonnull self, VirtualProcessor* _Nonnull vp, int reason, bool allowContextSwitch)
{
    VirtualProcessorScheduler* ps = gVirtualProcessorScheduler;
    bool isReady;


    // Nothing to do if we are not waiting
    if (vp->sched_state != kVirtualProcessorState_Waiting) {
        return false;
    }
    

    // Do not wake up the virtual processor if it is in an uninterruptible wait.
    if (reason == WAKEUP_REASON_INTERRUPTED && (vp->flags & VP_FLAG_INTERRUPTABLE_WAIT) == 0) {
        return false;
    }


    // Finish the wait. Remove the VP from the wait queue, the timeout queue and
    // store the wake reason.
    List_Remove(&self->q, &vp->rewa_queue_entry);
    
    VirtualProcessorScheduler_CancelTimeout(ps, vp);
    
    vp->waiting_on_wait_queue = NULL;
    vp->wakeup_reason = reason;
    vp->flags &= ~VP_FLAG_INTERRUPTABLE_WAIT;
    
    
    if (vp->suspension_count == 0) {
        // Make the VP ready and adjust it's effective priority based on the
        // time it has spent waiting
        const int32_t quartersSlept = (MonotonicClock_GetCurrentQuantums() - vp->wait_start_time) / ps->quantums_per_quarter_second;
        const int8_t boostedPriority = __min(vp->effectivePriority + __min(quartersSlept, VP_PRIORITY_HIGHEST), VP_PRIORITY_HIGHEST);
        VirtualProcessorScheduler_AddVirtualProcessor_Locked(ps, vp, boostedPriority);
        
        if (allowContextSwitch) {
            VirtualProcessorScheduler_MaybeSwitchTo(ps, vp);
        }
        isReady = true;
    } else {
        // The VP is suspended. Move it to ready state so that it will be
        // added to the ready queue once we resume it.
        vp->sched_state = kVirtualProcessorState_Ready;
        isReady = false;
    }

    return isReady;
}

// Wakes up either one or all waiters on the wait queue. The woken up VPs are
// removed from the wait queue. Expects to be called with preemption disabled.
void WaitQueue_Wakeup(WaitQueue* _Nonnull self, int flags, int reason)
{
    register ListNode* cp = self->q.first;
    register bool isWakeupOne = ((flags & WAKEUP_ONE) == WAKEUP_ONE);
    VirtualProcessor* pRunCandidate = NULL;

    
    // Make all waiting VPs ready and find a VP to potentially context switch to.
    while (cp) {
        register ListNode* np = cp->next;
        register VirtualProcessor* vp = (VirtualProcessor*)cp;
        register const bool isReady = WaitQueue_WakeupOne(self, vp, reason, false);

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
void WaitQueue_WakeupAllFromInterrupt(WaitQueue* _Nonnull self)
{
    register ListNode* cp = self->q.first;    
    
    // Make all waiting VPs ready to run but do not trigger a context switch.
    while (cp) {
        register ListNode* np = cp->next;
        
        WaitQueue_WakeupOne(self, (VirtualProcessor*)cp, WAKEUP_REASON_FINISHED, false);
        cp = np;
    }
}

void WaitQueue_Signal(WaitQueue* _Nonnull self, int flags, unsigned int sigs)
{
    VirtualProcessorScheduler* ps = gVirtualProcessorScheduler;
    VirtualProcessor* vp = (VirtualProcessor*)ps->running;
    const uint32_t fsigs = sigs & vp->sigmask;

    if (fsigs) {
        vp->psigs |= fsigs;

        WaitQueue_Wakeup(self, flags, WAKEUP_REASON_SIGNALLED);
    }
}



void WaitQueue_SuspendOne(WaitQueue* _Nonnull self, struct VirtualProcessor* _Nonnull vp)
{
    // We do not interrupt the wait because we'll just treat it as
    // a longer-than-expected wait. However we suspend the timeout
    // while the VP is suspended. The resume will reactive the
    // timeout and extend it by the amount of time that the VP has
    // spent in suspended state.
    VirtualProcessorScheduler_SuspendTimeout(gVirtualProcessorScheduler, vp);
}

void WaitQueue_ResumeOne(WaitQueue* _Nonnull self, struct VirtualProcessor* _Nonnull vp)
{
    // Still in waiting state -> just resume the timeout if one is
    // associated with the wait.
    VirtualProcessorScheduler_ResumeTimeout(gVirtualProcessorScheduler, vp, vp->suspension_time);
}
