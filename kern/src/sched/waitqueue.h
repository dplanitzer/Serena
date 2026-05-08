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
#include <stdbool.h>
#include <ext/queue.h>
#include <ext/nanotime.h>
#include <ext/try.h>
#include <kpi/signal.h>
#include <kpi/waitqueue.h>


struct vcpu;


// wait() options
#define WAIT_ABSTIME        2


// wakeup() options
#define WAKEUP_ALL      0   /* Requests that wakeup() wakes up all vcpus on the wait queue. */
#define WAKEUP_ONE      1   /* Requests that wakeup() wakes up at most one vcpu instead of all. */
#define WAKEUP_TIMEOUT  2   /* This is a timedwait() timeout wakeup */

// Do not allow the wakeup() call to immediately context switch to the waiting
// vcpu even if the QoS class and the current execution context would in
// principle allow for such a switch to happen. The waiting vcpu will still be
// marked ready and added to teh ready queue. The context switch will be
// deferred until preemption is reenabled and the quantum of the calling vcpu
// has expired.
#define WAKEUP_NO_IMMED_CSW  4


// Wait result/wakeup reason
typedef int8_t  wres_t;

#define WRES_WAKEUP     1
#define WRES_SIGNAL     2
#define WRES_TIMEOUT    3


struct waitqueue {
    deque_t q;
};
typedef struct waitqueue* waitqueue_t;



extern void wq_init(waitqueue_t _Nonnull self);

// Returns EBUSY and leaves the queue initialized, if there are still waiters on
// the wait queue.
extern errno_t wq_deinit(waitqueue_t _Nonnull self);


//
// Low-level primitives (only use these to implement higher level wait/wake APIs)
//

// Blocks the caller until it is woken up by a wq_wakeup_np() or
// wq_wakeup_many_np() call.
// @Entry Condition: preemption disabled
// @Entry Condition: 'vp' must be in running state
extern void wq_wait_np(waitqueue_t _Nonnull self);

// Blocks the caller until it is woken up by a wq_wakeup_np() or
// wq_wakeup_many_np() call. Also schedules a timeout timer that will trigger a
// wq_wakeup_np() when it expires. Returns true on a timeout and false otherwise.
// @Entry Condition: preemption disabled
// @Entry Condition: 'vp' must be in running state
extern bool wq_timedwait_np(waitqueue_t _Nonnull self, int flags, const nanotime_t* _Nullable wtp, nanotime_t* _Nullable rmtp);

// Wakes the vcpu 'vp' up if it is currently in wait state. Does nothing
// otherwise.
// @Entry Condition: preemption disabled
extern void wq_wakeup_vcpu_np(waitqueue_t _Nonnull self, struct vcpu* _Nonnull vp, int flags, int pri_boost);

// Wakes one or all vcpus on the wait queue up.
// @Entry Condition: preemption disabled
extern void wq_wakeup_many_np(waitqueue_t _Nonnull self, int flags, int pri_boost);

#endif /* _WAITQUEUE_H */
