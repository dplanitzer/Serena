//
//  sys/condvar.h
//  libc
//
//  Created by Dietmar Planitzer on 3/21/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef _SYS_CONDVAR_H
#define _SYS_CONDVAR_H 1

#include <signal.h>
#include <time.h>
#include <sys/mtx.h>
#include <sys/spinlock.h>

__CPP_BEGIN

typedef struct cond {
    spinlock_t  spinlock;
    int         wait_queue;
    int         signature;
} cond_t;


// Initializes a condition variable object.
extern int cond_init(cond_t* _Nonnull cv);

// Deinitializes the given condition variable.
extern int cond_deinit(cond_t* _Nonnull cv);


// Signals the given condition variable. Signaling a condition variable will
// wake up one waiter.
// @Concurrency: Safe
extern int cond_signal(cond_t* _Nonnull cv);

// Broadcasts the given condition variable. Broadcasting a condition variable
// will wake up all waiters.
// @Concurrency: Safe
extern int cond_broadcast(cond_t* _Nonnull cv);

// Blocks the caller until the given condition variable has been signaled or
// broadcast. Atomically unlocks 'mutex' and enters the wait state. Acquires 
// 'mutex' after wakeup.
// @Concurrency: Safe
extern int cond_wait(cond_t* _Nonnull cv, mtx_t* _Nullable mutex);

// Blocks the caller until the given condition variable has been signaled or
// broadcast. Atomically unlocks 'mutex' and enters the wait state. Acquires 
// 'mutex' after wakeup. 'wtp' may be a relative or absolute timeout value.
// Returns 0 on success and -1 and errno set to ETIMEOUT if the condition
// variable isn't signaled before the point in time defined by 'wtp'.
// @Concurrency: Safe
extern int cond_timedwait(cond_t* _Nonnull cv, mtx_t* _Nullable mutex, int flags, const struct timespec* _Nonnull wtp);

__CPP_END

#endif /* _SYS_CONDVAR_H */
