//
//  waitqueue.h
//  kernel
//
//  Created by Dietmar Planitzer on 6/28/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _WAITQUEUE_H
#define _WAITQUEUE_H 1

#include <kern/errno.h>
#include <kern/timespec.h>
#include <kern/types.h>
#include <kern/limits.h>
#include <klib/List.h>
#include <kpi/signal.h>
#include <kpi/waitqueue.h>


struct vcpu;


// wait() options
#define WAIT_ABSTIME        2


// Requests that wakeup() wakes up all vcpus on the wait queue.
#define WAKEUP_ALL  0

// Requests that wakeup() wakes up at most one vcpu instead of all.
#define WAKEUP_ONE  1

// Allow wakeup() to do a context switch
#define WAKEUP_CSW  2


// Wait result/wakeup reason
typedef int8_t  wres_t;

#define WRES_WAKEUP     1
#define WRES_SIGNAL     2
#define WRES_TIMEOUT    3


struct waitqueue {
    List    q;
};
typedef struct waitqueue* waitqueue_t;



extern void wq_init(waitqueue_t _Nonnull self);

// Returns EBUSY and leaves the queue initialized, if there are still waiters on
// the wait queue.
extern errno_t wq_deinit(waitqueue_t _Nonnull self);


// Checks whether the caller has signals pending and returns immediately if that's
// the case. Otherwise puts the caller to sleep until the a wakup() is executed
// by some other VP. Note that this function does not consume/clear any pending
// signals. It's the responsibility of the caller to do this if desired.
// @Entry Condition: preemption disabled
extern errno_t wq_wait(waitqueue_t _Nonnull self, const sigset_t* _Nullable mask);

// @Entry Condition: preemption disabled
extern errno_t wq_sigwait(waitqueue_t _Nonnull self, const sigset_t* _Nonnull set, siginfo_t* _Nullable info);

// Same as wait() but with support for timeouts. If 'wtp' is not NULL then 'wtp' is
// either the maximum duration to wait or the absolute time until to wait. The
// WAIT_ABSTIME specifies an absolute time. 'rmtp' is an optional timespec that
// receives the amount of time remaining if the wait was canceled early.
extern errno_t wq_timedwait(waitqueue_t _Nonnull self, const sigset_t* _Nullable mask, int flags, const struct timespec* _Nullable wtp, struct timespec* _Nullable rmtp);

// @Entry Condition: preemption disabled
extern errno_t wq_sigtimedwait(waitqueue_t _Nonnull self, const sigset_t* _Nonnull set, int flags, const struct timespec* _Nonnull wtp, siginfo_t* _Nullable info);


// Wakes up 'vp' if it is currently in waiting state. The wakeup reason is
// specified by 'reason'. 'flags' controls whether context switching to 'vp'
// is allowed or should not be done. 
// @Interrupt Context: Safe
// @Entry Condition: preemption disabled
extern bool wq_wakeone(waitqueue_t _Nonnull self, struct vcpu* _Nonnull vp, int flags, wres_t reason);

// Wakes up either one or all waiters on the wait queue. The woken up VPs are
// removed from the wait queue.
// @Entry Condition: preemption disabled
extern void wq_wake(waitqueue_t _Nonnull self, int flags, wres_t reason);

// Wakes up all VPs on the wait queue. Expects to be called from an interrupt
// context and thus defers context switches until the return from the interrupt
// context.
// @Entry Condition: preemption disabled
// @Interrupt Context: Safe
extern void wq_wake_irq(waitqueue_t _Nonnull self);


// Suspends an ongoing wait. This should be called if a VP that is currently
// waiting on this queue is suspended.
// @Entry Condition: preemption disabled
extern void wq_suspendone(waitqueue_t _Nonnull self, struct vcpu* _Nonnull vp);

// Resumes an ongoing wait. This should be called if a VP that is currently
// waiting on this queue is resumed.
// @Entry Condition: preemption disabled
extern void wq_resumeone(waitqueue_t _Nonnull self, struct vcpu* _Nonnull vp);

#endif /* _WAITQUEUE_H */
