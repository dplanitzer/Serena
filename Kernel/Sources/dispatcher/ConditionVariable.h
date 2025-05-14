//
//  ConditionVariable.h
//  kernel
//
//  Created by Dietmar Planitzer on 4/7/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef ConditionVariable_h
#define ConditionVariable_h

#include "Lock.h"


typedef struct ConditionVariable {
    List    wait_queue;
} ConditionVariable;


// Initializes a new condition variable.
extern void ConditionVariable_Init(ConditionVariable* _Nonnull pCondVar);

// Deinitializes the condition variables. All virtual processors that are still
// waiting on the conditional variable are woken up with an EINTR error.
extern void ConditionVariable_Deinit(ConditionVariable* _Nonnull pCondVar);

// Signals the condition variable. This will wake up one waiter.
#define ConditionVariable_Signal(__self) \
ConditionVariable_WakeAndUnlock(__self, NULL, false)

// Broadcasts the condition variable. This will wake up all waiters.
#define ConditionVariable_Broadcast(__self) \
ConditionVariable_WakeAndUnlock(__self, NULL, true)

// Signals the condition variable and optionally unlocks the provided lock. This
// will wake up one waiter.
#define ConditionVariable_SignalAndUnlock(__self, __pLock) \
ConditionVariable_WakeAndUnlock(__self, __pLock, false)

// Broadcasts the condition variable and optionally unlocks the provided lock.
// This will wake up all waiters.
#define ConditionVariable_BroadcastAndUnlock(__self, __pLock) \
ConditionVariable_WakeAndUnlock(__self, __pLock, true)

// Wakes up one or all waiters on the condition variable and optionally unlocks
// the provided lock.
extern void ConditionVariable_WakeAndUnlock(ConditionVariable* _Nonnull pCondVar, Lock* _Nullable pLock, bool broadcast);

// Blocks the caller until the condition variable has received a signal or the
// wait has timed out. Note that this function may return EINTR which means that
// the ConditionVariable_Wait() call is happening in the context of a system
// call that should be aborted.
extern errno_t ConditionVariable_Wait(ConditionVariable* _Nonnull pCondVar, Lock* _Nonnull pLock, TimeInterval deadline);

#endif /* ConditionVariable_h */
