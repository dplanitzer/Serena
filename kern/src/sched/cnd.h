//
//  cnd.h
//  kernel
//
//  Created by Dietmar Planitzer on 4/7/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef _CND_H
#define _CND_H 1

#include <sched/mtx.h>
#include <sched/waitqueue.h>


// Note: Interruptable
typedef struct cnd {
    struct waitqueue    wq;
} cnd_t;


// Initializes a new condition variable.
extern void cnd_init(cnd_t* _Nonnull self);

// Deinitializes the condition variables.
extern void cnd_deinit(cnd_t* _Nonnull self);

// Signals the condition variable. This will wake up one waiter.
#define cnd_signal(__self) \
_cnd_wake(__self, false)

// Broadcasts the condition variable. This will wake up all waiters.
#define cnd_broadcast(__self) \
_cnd_wake(__self, true)

// Blocks the caller until the condition variable has received a signal or the
// wait has timed out. Note that this function may return EINTR which means that
// the cnd_wait() call is happening in the context of a system call that should
// be aborted.
extern errno_t cnd_wait(cnd_t* _Nonnull self, mtx_t* _Nonnull mtx);

// Version of Wait() with an absolute timeout.
extern errno_t cnd_timedwait(cnd_t* _Nonnull self, mtx_t* _Nonnull mtx, const struct timespec* _Nonnull deadline);


// Wakes up one or all waiters on the condition variable.
extern void _cnd_wake(cnd_t* _Nonnull self, bool broadcast);

#endif /* _CND_H */
