//
//  sys/condvar.h
//  libc
//
//  Created by Dietmar Planitzer on 3/21/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef _SYS_CONDVAR_H
#define _SYS_CONDVAR_H 1

#include <kern/_cmndef.h>
#include <time.h>
#include <sys/errno.h>
#include <sys/mutex.h>

__CPP_BEGIN

typedef struct cond {
    int d[4];
} cond_t;


// Initializes a condition variable object.
extern errno_t cond_init(cond_t* _Nonnull cv);

// Deinitializes the given condition variable.
extern errno_t cond_deinit(cond_t* _Nonnull cv);


// Signals the given condition variable and optionally unlocks the given lock
// if it is not NULL. Signaling a condition variable will wake up one waiter.
// @Concurrency: Safe
extern errno_t cond_signal(cond_t* _Nonnull cv, mutex_t* _Nullable mutex);

// Broadcasts the given condition variable and optionally unlocks the given lock
// if it is not NULL. Broadcasting a condition variable will wake up all waiters.
// @Concurrency: Safe
extern errno_t cond_broadcast(cond_t* _Nonnull cv, mutex_t* _Nullable mutex);

// Blocks the caller until the given condition variable has been signaled or
// broadcast. Automatically and atomically acquires the lock 'lock'.
// @Concurrency: Safe
extern errno_t cond_wait(cond_t* _Nonnull cv, mutex_t* _Nullable mutex);

// Blocks the caller until the given condition variable has been signaled or
// broadcast. Automatically and atomically acquires the lock 'lock'. Returns EOK
// on success and ETIMEOUT if the condition variable isn't signaled before
// 'deadline'.
// @Concurrency: Safe
extern errno_t cond_timedwait(cond_t* _Nonnull cv, mutex_t* _Nullable mutex, const struct timespec* _Nonnull deadline);

__CPP_END

#endif /* _SYS_CONDVAR_H */
