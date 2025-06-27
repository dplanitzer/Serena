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

__CPP_BEGIN

// Requests that wakeup() wakes up all vcpus on the wait queue.
#define WAKE_ALL    0

// Requests that wakeup() wakes up at most one vcpu instead of all.
#define WAKE_ONE    1


// Creates a wait queue. Returns the wait queue descriptor on success and -1 on
// failure. Call destroy() to free the wait queue.
extern int waq_create(void);

// Waits on the wait queue until another vcpu calls wakeup() on the queue.
extern int waq_wait(int q);

// Waits on the wait queue until another vcpu calls wakeup() on the queue or
// until teh timeout 'wtp' is reached. Whatever comes first. Returns the unslept
// time in 'rmtp' if 'rmtp' is not NULL. 'wtp' is by default interpreted as a
// duration that will be added to the current time. Pass TIMER_ABSTIME if 'wtp'
// should be interpreted as an absolute point in time instead.
extern int waq_timedwait(int q, int flags, const struct timespec* _Nonnull wtp, struct timespec* _Nullable rmtp);

// Wakes up either one or all vcpus waiting on the wait queue.  
extern int waq_wakeup(int q, int flags);

__CPP_END

#endif /* _SYS_WAITQUEUE_H */
