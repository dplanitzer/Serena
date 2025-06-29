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
#include <kpi/waitqueue.h>

__CPP_BEGIN

// A wait queue allows a set of vcpus to wait for something interesting to
// happen and for one or multiple vcpus to notify waiting vcpus that something
// interesting has happened. Waiting vcpus are put to sleep and they consume no
// real CPU time until they are woken up.
//
// Wait queues support two different models of waiting:
// - stateless: a vcpu has to be in waiting state in order to wake it up. If
//              vcpu A executes a wakeup() and vcpu B is not currently in a
//              wait(), then vcpu B will miss the wakeup for good.
// - stateful: this waiting model ensures that a vcpu that isn't in a sigwait()
//             while a signal() is happening, will learn about the signal once
//             it calls sigwait().
//
// Expressed differently, a stateful waiting model ensures that the fact that a
// wakeup has happened will not be missed even if the wait state is entered
// after the wakeup. The fact that the vcpu needs to be awake is not lost.
//
// Every vcpu has a set of 32 pending signals and a signal mask. By default all
// signals are enabled. Use the vcpu_setsigmask() to change the signal mask.
// Only signals that are in the signal mask will wake up a vcpu that is waiting
// in a sigwait() call.


// Creates a wait queue with wait policy 'policy'. Returns the wait queue
// descriptor on success and -1 on failure. Call destroy() to free the wait
// queue.
extern int wq_create(int policy);


// Waits on the wait queue until another vcpu calls wakeup() on the queue.
extern int wq_wait(int q);

// Waits on the wait queue until another vcpu calls wakeup() on the queue or
// until the timeout 'wtp' is reached. Whatever comes first. 'wtp' is by default
// interpreted as a duration that will be added to the current time. Pass
// TIMER_ABSTIME if 'wtp' should be interpreted as an absolute point in time
// instead.
extern int wq_timedwait(int q, int flags, const struct timespec* _Nonnull wtp);

// Wakes up either one or all vcpus waiting on the wait queue.  
extern int wq_wakeup(int q, int flags);



// Waits on the wait queue if no signals are pending. Wakes up as soon as a signal
// is sent to the wait queue. Returns the pending signals in 'psigs'. Note that
// this calls consumes all pending signals.
extern int wq_sigwait(int q, unsigned int* _Nullable psigs);

// Waits on the wait queue until another vcpu calls wakeup() on the queue or
// until the timeout 'wtp' is reached. Whatever comes first. 'wtp' is by default
// interpreted as a duration that will be added to the current time. Pass
// TIMER_ABSTIME if 'wtp' should be interpreted as an absolute point in time
// instead.
extern int wq_sigtimedwait(int q, int flags, const struct timespec* _Nonnull wtp, unsigned int* _Nullable psigs);

// Sends signals to all vcpus waiting on the wait queue. 'sigs' is the set of
// signals that should be sent. These signals are merged into the pending signal
// set.
extern int wq_signal(int q, int flags, unsigned int sigs);

__CPP_END

#endif /* _SYS_WAITQUEUE_H */
