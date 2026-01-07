//
//  waitqueue.h
//  kernel
//
//  Created by Dietmar Planitzer on 6/28/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _WAITQUEUE_H
#define _WAITQUEUE_H 1

#include <limits.h>
#include <ext/queue.h>
#include <ext/timespec.h>
#include <ext/try.h>
#include <kern/types.h>
#include <kpi/signal.h>
#include <kpi/waitqueue.h>


struct vcpu;


// wait() options
#define WAIT_ABSTIME        2


// Requests that wakeup() wakes up all vcpus on the wait queue.
#define WAKEUP_ALL  0

// Requests that wakeup() wakes up at most one vcpu instead of all.
#define WAKEUP_ONE  1

// Allow wakeup() to do a context switch. Note that if this flag is combined
// with WAKEUP_IRQ that the actual context switch is deferred until the
// caller returns from the interrupt context 
#define WAKEUP_CSW  2

// Wakeup triggered from interrupt context
#define WAKEUP_IRQ  4


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


// The basic non-time-limited wait primitive. This function waits on the wait
// queue until it is explicitly woken up by one of the wake() calls or a signal
// arrives that is in the signal set 'set'. Note that 'set' is accepted as is
// and this function does _not_ ensure that non-maskable signals are added to
// 'set'. It's your responsibility to do this if so desired. Enables just
// non-maskable signals if 'set' is NULL.
// @Entry Condition: preemption disabled
// @Entry Condition: 'vp' must be in running state
extern wres_t wq_prim_wait(waitqueue_t _Nonnull self, const sigset_t* _Nullable set, bool armTimeout);

// Same as wq_prim_wait() but cancels the wait once the wait deadline specified
// by 'wtp' has arrived.
// @Entry Condition: preemption disabled
extern wres_t wq_prim_timedwait(waitqueue_t _Nonnull self, const sigset_t* _Nullable mask, int flags, const struct timespec* _Nullable wtp, struct timespec* _Nullable rmtp);


// Checks whether the caller has signals pending that are members of the signal
// set 'set' and returns immediately if that's the case. Note that 'set' is
// taken verbatim if provided. This function does not automatically add the
// non-maskable signals to the set if they are missing. It is your responsibility
// to do that if so desired. Puts the caller to sleep until a wakeup() is executed
// by some other VP. Note that this function does not consume/clear any pending
// signals. It's the responsibility of the caller to do this if so desired.
// Note: most callers should pass NULL for 'set'. Passing something else is a
// special case that is only relevant if you do not want to be woken up by a
// vcpu abort.
// @Entry Condition: preemption disabled
extern errno_t wq_wait(waitqueue_t _Nonnull self, const sigset_t* _Nullable set);

// Same as wait() but with support for timeouts. If 'wtp' is not NULL then 'wtp' is
// either the maximum duration to wait or the absolute time until to wait. The
// WAIT_ABSTIME specifies an absolute time. 'rmtp' is an optional timespec that
// receives the amount of time remaining if the wait was canceled early.
extern errno_t wq_timedwait(waitqueue_t _Nonnull self, const sigset_t* _Nullable set, int flags, const struct timespec* _Nullable wtp, struct timespec* _Nullable rmtp);


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

#endif /* _WAITQUEUE_H */
