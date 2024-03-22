//
//  ConditionVariable.h
//  libsystem
//
//  Created by Dietmar Planitzer on 3/21/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef _SYS_CONDITION_VARIABLE_H
#define _SYS_CONDITION_VARIABLE_H 1

#include <System/_cmndef.h>
#include <System/Error.h>
#include <System/Types.h>
#include <System/TimeInterval.h>
#include <System/Lock.h>

__CPP_BEGIN

#if !defined(__KERNEL__)

typedef struct ConditionVariable {
    char    d[16];
} ConditionVariable;
typedef ConditionVariable* ConditionVariableRef;


// Initializes a condition variable object.
extern errno_t ConditionVariable_Init(ConditionVariableRef _Nonnull cv);

// Deinitializes the given condition variable.
extern errno_t ConditionVariable_Deinit(ConditionVariableRef _Nonnull cv);


// Signals the given condition variable and optionally unlocks the given lock
// if it is not NULL. Signaling a condition variable will wake up one waiter.
// @Concurrency: Safe
extern errno_t ConditionVariable_Signal(ConditionVariableRef _Nonnull cv, LockRef _Nullable lock);

// Broadcasts the given condition variable and optionally unlocks the given lock
// if it is not NULL. Broadcasting a condition variable will wake up all waiters.
// @Concurrency: Safe
extern errno_t ConditionVariable_Broadcast(ConditionVariableRef _Nonnull cv, LockRef _Nullable lock);

// Blocks the caller until the given condition variable has been signaled or
// broadcast. Automatically and atomically acquires the lock 'lock'. Returns EOK
// on success and ETIMEOUT if the condition variable isn't signaled before
// 'deadline'.
// @Concurrency: Safe
extern errno_t ConditionVariable_Wait(ConditionVariableRef _Nonnull cv, LockRef _Nonnull lock, TimeInterval deadline);

#endif /* __KERNEL__ */

__CPP_END

#endif /* _SYS_CONDITION_VARIABLE_H */
