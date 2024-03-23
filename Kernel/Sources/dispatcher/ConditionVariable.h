//
//  ConditionVariable.h
//  kernel
//
//  Created by Dietmar Planitzer on 4/7/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef ConditionVariable_h
#define ConditionVariable_h

#include <klib/klib.h>
#include "Lock.h"


// A condition variable
typedef struct _ConditionVariable {
    List    wait_queue;
} ConditionVariable;


// Initializes a new condition variable.
extern void ConditionVariable_Init(ConditionVariable* _Nonnull pCondVar);

// Deinitializes the condition variables. All virtual processors that are still
// waiting on the conditional variable are woken up with an EINTR error.
extern void ConditionVariable_Deinit(ConditionVariable* _Nonnull pCondVar);

extern void ConditionVariable_SignalAndUnlock(ConditionVariable* _Nonnull pCondVar, Lock* _Nullable pLock);
extern void ConditionVariable_BroadcastAndUnlock(ConditionVariable* _Nonnull pCondVar, Lock* _Nullable pLock);

// Blocks the caller until the condition variable has received a signal or the
// wait has timed out. Note that this function may return EINTR which means that
// the ConditionVariable_Wait() call is happening in the context of a system
// call that should be aborted.
extern errno_t ConditionVariable_Wait(ConditionVariable* _Nonnull pCondVar, Lock* _Nonnull pLock, TimeInterval deadline);

#endif /* ConditionVariable_h */
