//
//  sys/waitqueue.h
//  libc
//
//  Created by Dietmar Planitzer on 6/26/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _SYS_WAITQUEUE_H
#define _SYS_WAITQUEUE_H 1

#include <_cmndef.h>
#include <_null.h>
#include <kpi/_time.h>
#include <kpi/signal.h>
#include <kpi/waitqueue.h>

__CPP_BEGIN

// A wait queue allows a set of vcpus to wait for something interesting to
// happen and for one or multiple vcpus to notify waiting vcpus that something
// interesting has happened. Waiting vcpus are put to sleep and they consume no
// real CPU time until they are woken up.


// Creates a wait queue with wait policy 'policy'. Returns the wait queue
// descriptor on success and -1 on failure. Call destroy() to free the wait
// queue.
extern int wq_create(int policy);


// Waits on the wait queue until another vcpu calls wakeup() on the queue.
extern int wq_wait(int q);

// Atomically replaces the current signal mask with 'mask' and waits for the
// arrival of a signal that is not blocked by the signal mask in effect. All
// unblocked signals are returned and cleared from the pending signal set. If
// 'mask' is NULL then the current signal mask is used. The original signal mask
// is restored after the wait has completed.
extern int wq_sigwait(int q, const sigset_t* _Nullable mask, sigset_t* _Nonnull sigs);

// Waits on the wait queue until another vcpu calls wakeup() on the queue or
// until the timeout 'wtp' is reached. Whatever comes first. 'wtp' is by default
// interpreted as a duration that will be added to the current time. Pass
// TIMER_ABSTIME if 'wtp' should be interpreted as an absolute point in time
// instead.
extern int wq_timedwait(int q, int flags, const struct timespec* _Nonnull wtp);

// Like sig_wait() but limits the waiting time to the timeout 'wtp'. 'wtp' is by
// default interpreted as a duration that will be added to the current time. Pass
// TIMER_ABSTIME if 'wtp' should be interpreted as an absolute point in time
// instead.
extern int wq_sigtimedwait(int q, const sigset_t* _Nullable mask, sigset_t* _Nonnull sigs, int flags, const struct timespec* _Nonnull wtp);

// Wakes up a single waiter or all waiters on the wait queue depending on how
// the 'flags' are configured. 'signo' must be a valid signal number in order
// to wake up a sigwait(), sigtimedwait(), wait() or timedwait(). If 'signo' is
// 0 then this function will only wake up a wait() or timedwait() but not a
// sigwait() nor a sigtimedwait(). 
extern int wq_wakeup(int q, int flags, int signo);

__CPP_END

#endif /* _SYS_WAITQUEUE_H */
