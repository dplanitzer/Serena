//
//  ConditionVariable.h
//  kernel
//
//  Created by Dietmar Planitzer on 4/7/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef ConditionVariable_h
#define ConditionVariable_h

#include <sched/mtx.h>
#include <sched/waitqueue.h>


// Note: Interruptable
typedef struct ConditionVariable {
    struct waitqueue    wq;
} ConditionVariable;


// Initializes a new condition variable.
extern void ConditionVariable_Init(ConditionVariable* _Nonnull self);

// Deinitializes the condition variables.
extern void ConditionVariable_Deinit(ConditionVariable* _Nonnull self);

// Signals the condition variable. This will wake up one waiter.
#define ConditionVariable_Signal(__self) \
_ConditionVariable_Wakeup(__self, false)

// Broadcasts the condition variable. This will wake up all waiters.
#define ConditionVariable_Broadcast(__self) \
_ConditionVariable_Wakeup(__self, true)

// Blocks the caller until the condition variable has received a signal or the
// wait has timed out. Note that this function may return EINTR which means that
// the ConditionVariable_Wait() call is happening in the context of a system
// call that should be aborted.
extern errno_t ConditionVariable_Wait(ConditionVariable* _Nonnull self, mtx_t* _Nonnull pLock);

// Version of Wait() with an absolute timeout.
extern errno_t ConditionVariable_TimedWait(ConditionVariable* _Nonnull self, mtx_t* _Nonnull pLock, const struct timespec* _Nonnull deadline);


// Wakes up one or all waiters on the condition variable.
extern void _ConditionVariable_Wakeup(ConditionVariable* _Nonnull self, bool broadcast);

#endif /* ConditionVariable_h */
