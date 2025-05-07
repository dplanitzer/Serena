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
#include <System/Mutex.h>

__CPP_BEGIN

typedef struct os_cond {
    int d[4];
} os_cond_t;


// Initializes a condition variable object.
extern errno_t os_cond_init(os_cond_t* _Nonnull cv);

// Deinitializes the given condition variable.
extern errno_t os_cond_deinit(os_cond_t* _Nonnull cv);


// Signals the given condition variable and optionally unlocks the given lock
// if it is not NULL. Signaling a condition variable will wake up one waiter.
// @Concurrency: Safe
extern errno_t os_cond_signal(os_cond_t* _Nonnull cv, os_mutex_t* _Nullable mutex);

// Broadcasts the given condition variable and optionally unlocks the given lock
// if it is not NULL. Broadcasting a condition variable will wake up all waiters.
// @Concurrency: Safe
extern errno_t os_cond_broadcast(os_cond_t* _Nonnull cv, os_mutex_t* _Nullable mutex);

// Blocks the caller until the given condition variable has been signaled or
// broadcast. Automatically and atomically acquires the lock 'lock'.
// @Concurrency: Safe
extern errno_t os_cond_wait(os_cond_t* _Nonnull cv, os_mutex_t* _Nullable mutex);

// Blocks the caller until the given condition variable has been signaled or
// broadcast. Automatically and atomically acquires the lock 'lock'. Returns EOK
// on success and ETIMEOUT if the condition variable isn't signaled before
// 'deadline'.
// @Concurrency: Safe
extern errno_t os_cond_timedwait(os_cond_t* _Nonnull cv, os_mutex_t* _Nullable mutex, const TimeInterval* _Nonnull deadline);

__CPP_END

#endif /* _SYS_CONDITION_VARIABLE_H */
