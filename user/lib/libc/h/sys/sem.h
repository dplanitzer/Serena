//
//  sem.h
//  libc
//
//  Created by Dietmar Planitzer on 3/21/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef _SYS_SEM_H
#define _SYS_SEM_H 1

#include <_cmndef.h>
#include <time.h>
#include <sys/spinlock.h>

__CPP_BEGIN

typedef struct sem {
    spinlock_t  spinlock;
    int         permits;
    int         waiters;
    int         signature;
    int         wait_queue;
} sem_t;


// Initializes a semaphore object with the given number of permits.
extern int sem_init(sem_t* _Nonnull sema, int npermits);

// Deinitializes the given semaphore.
extern int sem_deinit(sem_t* _Nonnull sema);


// Relinquishes the given number of permits to the given semaphore. This makes
// the permits available for acquisition by other execution contexts. Note that
// 'npermits' must be > 0. ERANGE is returned if this is not the case.
// @Concurrency: Safe
extern int sem_post(sem_t* _Nonnull sema, int npermits);

// Blocks the caller until 'npermits' have been successfully acquired. Returns 0
// on success and -1 and on failure.
// @Concurrency: Safe
extern int sem_wait(sem_t* _Nonnull sema, int npermits);

// Like sem_wait() but allows the specification of a timeout value. 'wtp' is
// an absolute point in time if 'flags' includes TIMER_ABSTIME and a duration
// relative to the current time otherwise. 
// @Concurrency: Safe
extern int sem_timedwait(sem_t* _Nonnull self, int npermits, int flags, const struct timespec* _Nonnull wtp);

// Attempts to acquire 'npermits' without blocking. Returns 0 on success and -1
// and errno set to EBUSY otherwise.
// @Concurrency: Safe
extern int sem_trywait(sem_t* _Nonnull sema, int npermits);

__CPP_END

#endif /* _SYS_SEM_H */
