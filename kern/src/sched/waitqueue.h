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
#include <ext/atomic.h>
#include <ext/queue.h>
#include <ext/nanotime.h>
#include <ext/try.h>
#include <hal/clock.h>
#include <kpi/signal.h>
#include <kpi/synch.h>
#include <kpi/_time.h>


struct vcpu;


// wq_wait_addr() options
//#define TIMER_ABSTIME     2      declared in _time.h
#define SIGWAIT_NOABORT     4   /* informs wq_wait_addr(), vcpu_sigwait() that it should ignore system call aborts */


// wakeup() options (see kpi/synch.h for user visible options)
#define WAKEUP_TIMEOUT  2   /* This is a timedwait() timeout wakeup */

// Do not allow the wakeup() call to immediately context switch to the waiting
// vcpu even if the QoS class and the current execution context would in
// principle allow for such a switch to happen. The waiting vcpu will still be
// marked ready and added to teh ready queue. The context switch will be
// deferred until preemption is reenabled and the quantum of the calling vcpu
// has expired.
#define WAKEUP_NO_IMMED_CSW  4


struct waitqueue {
    deque_t q;
};
typedef struct waitqueue* waitqueue_t;

#define WAITQUEUE_INIT (struct waitqueue){0}


extern void wq_init(waitqueue_t _Nonnull self);

// Returns EBUSY and leaves the queue initialized, if there are still waiters on
// the wait queue.
extern errno_t wq_deinit(waitqueue_t _Nonnull self);


//
// Low-level primitives (only use these to implement higher level wait/wake APIs)
//

// Calculates the (absolute) deadline value for a wq_wait_np() call. 'clock' is
// the clock in which 'wtp' is expressed. 'wtp' is the absolute or relative wait
// timeout and 'flags' specifies whether 'wtp' is relative or absolute. Returns
// the calculated deadline value as an absolute ticks value in the time system
// of the scheduler clock if 'wtp' was less than infinity. Returns TICKS_MAX if
// 'wtp' is infinity. 
extern ticks_t wq_calc_deadline(clock_ref_t _Nonnull clock, int flags, const nanotime_t* _Nonnull wtp);

// Waits until wakeup() is called on the wait queue 'self' or until the deadline
// 'deadline' has been reached if 'deadline' is < TICKS_MAX. Note that the
// deadline should be calculated with the help of the wq_calc_deadline()
// function and that it is an absolute time value in the time system of the
// scheduler clock. Returns EOK on a regular (and potentially spurious) wakeup
// and ETIMEDOUT on a deadline wakeup.
// @Entry Condition: preemption disabled
extern errno_t wq_wait_np(waitqueue_t _Nonnull self, const ticks_t deadline);

// Waits as long as the memory location 'addr' contains the value 'expected'.
// Returns once this is no longer the case or SIG_FORCE_QUIT is pending and
// 'flags' does not contain SIGWAIT_NOABORT. Takes care of locking/unlocking the
// queue. Otherwise works like wq_wait_np().
extern errno_t wq_wait_addr(waitqueue_t _Nonnull self, volatile atomic_int* _Nonnull addr, int expected, int flags, const ticks_t deadline);


// Specifically wakes up the vcpu 'vp' if it is currently in wait state. Does
// nothing otherwise.
// @Entry Condition: preemption disabled
extern void wq_wakeup_vcpu_np(waitqueue_t _Nonnull self, struct vcpu* _Nonnull vp, int flags, int pri_boost);

// Wakes one or all vcpus on the wait queue up.
// @Entry Condition: preemption disabled
extern void wq_wakeup_np(waitqueue_t _Nonnull self, int flags, int pri_boost);

// Like wq_wakeup_np() but takes care of locking /unlocking the queue.
extern void wq_wakeup(waitqueue_t _Nonnull self, int flags, int pri_boost);

#endif /* _WAITQUEUE_H */
