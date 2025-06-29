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


// wait() options
#define WAIT_INTERRUPTABLE  1
#define WAIT_ABSTIME        2


// Requests that wakeup() wakes up all vcpus on the wait queue.
#define WAKEUP_ALL  0

// Requests that wakeup() wakes up at most one vcpu instead of all.
#define WAKEUP_ONE  1

// Allow wakeup() to do a context switch
#define WAKEUP_CSW  2


// Reason for a wake up
// WAKEUP_REASON_NONE means that we are still waiting for a wakeup
#define WAKEUP_REASON_NONE          0
#define WAKEUP_REASON_FINISHED      1
#define WAKEUP_REASON_INTERRUPTED   2
#define WAKEUP_REASON_TIMEOUT       3
#define WAKEUP_REASON_SIGNALLED     4


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
// @Entry Condition: preemption disabled
extern errno_t WaitQueue_Wait(WaitQueue* _Nonnull self, int flags);

// Same as wait() but will be woken up by signals. Returns the signals and clears
// the pending signals of the calling VP.
// @Entry Condition: preemption disabled
extern errno_t WaitQueue_SigWait(WaitQueue* _Nonnull self, int flags, unsigned int* _Nullable pOutSigs);

// Same as wait() but with support for timeouts. If 'wtp' is not NULL then 'wtp' is
// either the maximum duration to wait or the absolute time until to wait. The
// WAIT_ABSTIME specifies an absolute time. 'rmtp' is an optional timespec that
// receives the amount of time remaining if the wait was canceled early.
extern errno_t WaitQueue_TimedWait(WaitQueue* _Nonnull self, int flags, const struct timespec* _Nullable wtp, struct timespec* _Nullable rmtp);

// Same as timedwait() but will be woken up by signals. Returns the signals and
// clears the pending signals of the calling VP.
// @Entry Condition: preemption disabled
extern errno_t WaitQueue_SigTimedWait(WaitQueue* _Nonnull self, int flags, const struct timespec* _Nullable wtp, struct timespec* _Nullable rmtp, unsigned int* _Nullable pOutSigs);


// Adds the given VP from the given wait queue to the ready queue. The VP is removed
// from the wait queue. The scheduler guarantees that a wakeup operation will never
// fail with an error. This doesn't mean that calling this function will always
// result in a virtual processor wakeup. If the wait queue is empty then no wakeups
// will happen. Returns true if the vp has been made ready to run; false otherwise.
// @Entry Condition: preemption disabled
extern bool WaitQueue_WakeupOne(WaitQueue* _Nonnull self, struct VirtualProcessor* _Nonnull vp, int reason, bool allowContextSwitch);

// Wakes up up to 'count' waiters on the wait queue. The woken up VPs are
// removed from the wait queue. Expects to be called with preemption disabled.
// @Entry Condition: preemption disabled
extern void WaitQueue_Wakeup(WaitQueue* _Nonnull self, int flags, int reason);

// Adds all VPs on the given list to the ready queue. The VPs are removed from
// the wait queue. Expects to be called from an interrupt context and thus defers
// context switches until the return from the interrupt context.
// @Entry Condition: preemption disabled
extern void WaitQueue_WakeupAllFromInterrupt(WaitQueue* _Nonnull self);

// Sends a signal to the wait queue. This will wake up one or all VPs that have
// at least one of the signals enabled in their signal mask that is listed in
// 'sigs'. Does nothing if 'sigs' is 0 or none of the signals are enabled in the
// VPs signal mask.
// @Entry Condition: preemption disabled
extern void WaitQueue_Signal(WaitQueue* _Nonnull self, int flags, unsigned int sigs);


// Suspends an ongoing wait. This should be called if a VP that is currently
// waiting on this queue is suspended.
extern void WaitQueue_SuspendOne(WaitQueue* _Nonnull self, struct VirtualProcessor* _Nonnull vp);

// Resumes an ongoing wait. This should be called if a VP that is currently
// waiting on this queue is resumed.
extern void WaitQueue_ResumeOne(WaitQueue* _Nonnull self, struct VirtualProcessor* _Nonnull vp);

#endif /* WaitQueue_h */
