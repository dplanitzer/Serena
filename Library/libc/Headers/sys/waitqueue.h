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
#include <kpi/types.h>
#include <kpi/waitqueue.h>

__CPP_BEGIN

// A wait queue implements an edge-triggered wait mechanism. This means that
// wake ups are done without sending a signal and thus a wake up will only
// affect a virtual processor that is blocked on a wq_wait() or wq_timewait()
// at the time the wq_wakeup() call is executed. This kind of waiting model
// requires that you always write code in such a way that it maintains its own
// state that allows it to decide whether a wakeup was a spurious wakeup or an
// actual wakeup. The advantage of this waiting model is that it is very low
// overhead since it mostly ignores signals. Signals only matter in the sense
// that they may cause spurious wake ups. However the wai calls allow you to
// temporarily replace the signal mask of the virtual processor which should
// help in avoiding spurious wakeups caused by unexpected signals.


// Creates a wait queue with wait policy 'policy'. Returns the wait queue
// descriptor on success and -1 on failure. Call destroy() to free the wait
// queue.
extern int wq_create(int policy);


// Blocks the caller until an edge-triggered wakeup by wq_wakeup() is executed
// on this wait queue.
extern int wq_wait(int q);

// Same as wq_wait() but allows you to specify a timeout. The timeout is a
// duration by default. Pass TIMER_ABSTIME in 'flags' to make it an absolute
// time value. Returns ETIMEDOUT if the timeout is reached.
extern int wq_timedwait(int q, int flags, const struct timespec* _Nonnull wtp);

// Atomically wakes one waiter on wait queue 'oq' up and then enters the wait
// state on the wait queue 'q'. Otherwise just like wq_timedwait(). 
extern int wq_timedwakewait(int q, int oq, int flags, const struct timespec* _Nonnull wtp);

// Wakes up one or all waiters currently blocked on the wait queue 'q'. Note
// that this function does not send a signal. Thus it will only wake up waiters
// that are currently blocked in a wq_wait() or wq_timedwait() call.
extern int wq_wakeup(int q, int flags);

__CPP_END

#endif /* _SYS_WAITQUEUE_H */
