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
// @Entry Condition: 'vp' must not be in an active abort
static errno_t _do_wait(WaitQueue* _Nonnull self, int flags, VirtualProcessorScheduler* _Nonnull ps, VirtualProcessor* _Nonnull vp)
{
    // Put us on the wait queue. The wait queue is sorted by the QoS and priority
    // from highest to lowest. VPs which enter the queue first, leave it first.
    register VirtualProcessor* pvp = NULL;
    register VirtualProcessor* cvp = (VirtualProcessor*)self->q.first;
    while (cvp) {
        if (cvp->effectivePriority < vp->effectivePriority) {
            break;
        }
        
        pvp = cvp;
        cvp = (VirtualProcessor*)cvp->rewa_queue_entry.next;
    }
    
    List_InsertAfter(&self->q, &vp->rewa_queue_entry, &pvp->rewa_queue_entry);
    
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
        default:                        return EOK;
    }
}

errno_t WaitQueue_Wait(WaitQueue* _Nonnull self, int flags)
{
    VirtualProcessorScheduler* ps = gVirtualProcessorScheduler;
    VirtualProcessor* vp = (VirtualProcessor*)ps->running;

    assert(vp->sched_state == kVirtualProcessorState_Running);


    // Immediately return instead of waiting if we are in the middle of an abort
    // of a call-as-user invocation.
    if ((vp->flags & (VP_FLAG_CAU_IN_PROGRESS | VP_FLAG_CAU_ABORTED)) == (VP_FLAG_CAU_IN_PROGRESS | VP_FLAG_CAU_ABORTED)) {
        return EINTR;
    }


    return _do_wait(self, flags, ps, vp);
}

errno_t WaitQueue_TimedWait(WaitQueue* _Nonnull self, int flags, const struct timespec* _Nullable wtp, struct timespec* _Nullable rmtp)
{
    VirtualProcessorScheduler* ps = gVirtualProcessorScheduler;
    VirtualProcessor* vp = (VirtualProcessor*)ps->running;
    struct timespec now, deadline;

    // Immediately return instead of waiting if we are in the middle of an abort
    // of a call-as-user invocation.
    if ((vp->flags & (VP_FLAG_CAU_IN_PROGRESS | VP_FLAG_CAU_ABORTED)) == (VP_FLAG_CAU_IN_PROGRESS | VP_FLAG_CAU_ABORTED)) {
        if (rmtp) {
            timespec_clear(rmtp);   // User space won't see this value anyway
        }
        return EINTR;
    }


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
    }


    // Now wait
    const errno_t err = _do_wait(self, flags, ps, vp);


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

// Adds all VPs on the given list to the ready queue. The VPs are removed from
// the wait queue. Expects to be called from an interrupt context and thus defers
// context switches until the return from the interrupt context.
void WaitQueue_WakeUpAllFromInterrupt(WaitQueue* _Nonnull self)
{
    register ListNode* cnp = self->q.first;    
    
    // Make all waiting VPs ready to run but do not trigger a context switch.
    while (cnp) {
        register ListNode* nnp = cnp->next;
        
        WaitQueue_WakeUpOne(self, (VirtualProcessor*)cnp, WAKEUP_REASON_FINISHED, false);
        cnp = nnp;
    }
}

// Wakes up, up to 'count' waiters on the wait queue. The woken up VPs are
// removed from the wait queue. Expects to be called with preemption disabled.
void WaitQueue_WakeUpSome(WaitQueue* _Nonnull self, int count, int wakeUpReason, bool allowContextSwitch)
{
    register ListNode* pCurNode = self->q.first;
    register int i = 0;
    VirtualProcessor* pRunCandidate = NULL;
    
    
    // First pass: make all waiting VPs ready and collect as many VPs to run as
    // we got CPU cores on this machine.
    while (pCurNode && i < count) {
        register ListNode* pNextNode = pCurNode->next;
        register VirtualProcessor* vp = (VirtualProcessor*)pCurNode;
        
        WaitQueue_WakeUpOne(self, vp, wakeUpReason, false);
        if (pRunCandidate == NULL && (vp->sched_state == kVirtualProcessorState_Ready && vp->suspension_count == 0)) {
            pRunCandidate = vp;
        }
        pCurNode = pNextNode;
        i++;
    }
    
    
    // Second pass: start running all the VPs that we collected in pass one.
    if (allowContextSwitch && pRunCandidate) {
        VirtualProcessorScheduler_MaybeSwitchTo(gVirtualProcessorScheduler, pRunCandidate);
    }
}

// Adds the given VP from the given wait queue to the ready queue. The VP is
// removed from the wait queue. The scheduler guarantees that a wakeup operation
// will never fail with an error. This doesn't mean that calling this function
// will always result in a virtual processor wakeup. If the wait queue is empty
// then no wakeups will happen. Also a virtual processor that sits in an
// uninterruptible wait or that was suspended while being in a wait state will
// not get woken up.
// May be called from an interrupt context.
void WaitQueue_WakeUpOne(WaitQueue* _Nonnull self, VirtualProcessor* _Nonnull vp, int wakeUpReason, bool allowContextSwitch)
{
    VirtualProcessorScheduler* ps = gVirtualProcessorScheduler;


    // Nothing to do if we are not waiting
    if (vp->sched_state != kVirtualProcessorState_Waiting) {
        return;
    }
    

    // Do not wake up the virtual processor if it is in an uninterruptible wait.
    if (wakeUpReason == WAKEUP_REASON_INTERRUPTED && (vp->flags & VP_FLAG_INTERRUPTABLE_WAIT) == 0) {
        return;
    }


    // Finish the wait. Remove the VP from the wait queue, the timeout queue and
    // store the wake reason.
    List_Remove(&self->q, &vp->rewa_queue_entry);
    
    VirtualProcessorScheduler_CancelTimeout(ps, vp);
    
    vp->waiting_on_wait_queue = NULL;
    vp->wakeup_reason = wakeUpReason;
    vp->flags &= ~VP_FLAG_INTERRUPTABLE_WAIT;
    
    
    if (vp->suspension_count == 0) {
        // Make the VP ready and adjust it's effective priority based on the
        // time it has spent waiting
        const int32_t quatersSlept = (MonotonicClock_GetCurrentQuantums() - vp->wait_start_time) / ps->quantums_per_quarter_second;
        const int8_t boostedPriority = __min(vp->effectivePriority + __min(quatersSlept, VP_PRIORITY_HIGHEST), VP_PRIORITY_HIGHEST);
        VirtualProcessorScheduler_AddVirtualProcessor_Locked(ps, vp, boostedPriority);
        
        if (allowContextSwitch) {
            VirtualProcessorScheduler_MaybeSwitchTo(ps, vp);
        }
    } else {
        // The VP is suspended. Move it to ready state so that it will be
        // added to the ready queue once we resume it.
        vp->sched_state = kVirtualProcessorState_Ready;
    }
}

void WaitQueue_Suspend(WaitQueue* _Nonnull self, struct VirtualProcessor* _Nonnull vp)
{
    // We do not interrupt the wait because we'll just treat it as
    // a longer-than-expected wait. However we suspend the timeout
    // while the VP is suspended. The resume will reactive the
    // timeout and extend it by the amount of time that the VP has
    // spent in suspended state.
    VirtualProcessorScheduler_SuspendTimeout(gVirtualProcessorScheduler, vp);
}

void WaitQueue_Resume(WaitQueue* _Nonnull self, struct VirtualProcessor* _Nonnull vp)
{
    // Still in waiting state -> just resume the timeout if one is
    // associated with the wait.
    VirtualProcessorScheduler_ResumeTimeout(gVirtualProcessorScheduler, vp, vp->suspension_time);
}
