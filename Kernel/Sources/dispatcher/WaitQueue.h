//
//  WaitQueue.h
//  kernel
//
//  Created by Dietmar Planitzer on 6/28/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef WaitQueue_h
#define WaitQueue_h

#include <kern/errno.h>
#include <kern/timespec.h>
#include <kern/types.h>
#include <kern/limits.h>
#include <klib/List.h>

struct VirtualProcessor;


// Wait() options
#define WAIT_INTERRUPTABLE  1
#define WAIT_ABSTIME        2


// Reason for a wake up
// WAKEUP_REASON_NONE means that we are still waiting for a wake up
#define WAKEUP_REASON_NONE          0
#define WAKEUP_REASON_FINISHED      1
#define WAKEUP_REASON_INTERRUPTED   2
#define WAKEUP_REASON_TIMEOUT       3


typedef struct WaitQueue {
    List    q;
} WaitQueue;



extern void WaitQueue_Init(WaitQueue* _Nonnull self);

// Returns EBUSY and leaves the queue initialized, if there are still waiters on
// the wait queue.
extern errno_t WaitQueue_Deinit(WaitQueue* self);

// Put the currently running VP (the caller) on the given wait queue. Then runs
// the scheduler to select another VP to run and context switches to the new VP
// right away.
// Expects to be called with preemption disabled. Temporarily reenables
// preemption when context switching to another VP. Returns to the caller with
// preemption disabled.
// Waits until wakeup if 'wtp' is NULL. If 'wtp' is not NULL then 'wtp' is
// either the maximum duration to wait or the absolute time until to wait. The
// WAIT_ABSTIME specifies an absolute time. 'rmtp' is an optional timespec that
// receives the amount of time remaining if the wait was canceled early.
// @Entry Condition: preemption disabled
extern errno_t WaitQueue_Wait(WaitQueue* _Nonnull self, int flags, const struct timespec* _Nullable wtp, struct timespec* _Nullable rmtp);

// Adds the given VP from the given wait queue to the ready queue. The VP is removed
// from the wait queue. The scheduler guarantees that a wakeup operation will never
// fail with an error. This doesn't mean that calling this function will always
// result in a virtual processor wakeup. If the wait queue is empty then no wakeups
// will happen.
// @Entry Condition: preemption disabled
extern void WaitQueue_WakeUpOne(WaitQueue* _Nonnull self, struct VirtualProcessor* _Nonnull vp, int wakeUpReason, bool allowContextSwitch);

// Wakes up up to 'count' waiters on the wait queue. The woken up VPs are
// removed from the wait queue. Expects to be called with preemption disabled.
// @Entry Condition: preemption disabled
extern void WaitQueue_WakeUpSome(WaitQueue* _Nonnull self, int count, int wakeUpReason, bool allowContextSwitch);

// Adds all VPs on the given list to the ready queue. The VPs are removed from
// the wait queue.
// @Entry Condition: preemption disabled
#define WaitQueue_WakeUpAll(__self, __allowContextSwitch) \
    WaitQueue_WakeUpSome(__self, INT_MAX, WAKEUP_REASON_FINISHED, __allowContextSwitch);

// Adds all VPs on the given list to the ready queue. The VPs are removed from
// the wait queue. Expects to be called from an interrupt context and thus defers
// context switches until the return from the interrupt context.
// @Entry Condition: preemption disabled
extern void WaitQueue_WakeUpAllFromInterruptContext(WaitQueue* _Nonnull self);

// Suspends an ongoing wait. This should be called if a VP that is currently
// waiting on this queue is suspended.
extern void WaitQueue_Suspend(WaitQueue* _Nonnull self, struct VirtualProcessor* _Nonnull vp);

// Resumes an ongoing wait. This should be called if a VP that is currently
// waiting on this queue is resumed.
extern void WaitQueue_Resume(WaitQueue* _Nonnull self, struct VirtualProcessor* _Nonnull vp);

#endif /* WaitQueue_h */
