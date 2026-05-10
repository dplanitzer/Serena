//
//  sem.h
//  libc
//
//  Created by Dietmar Planitzer on 3/21/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef _SERENA_SEM_H
#define _SERENA_SEM_H 1

#include <_cmndef.h>
#include <ext/atomic.h>
#include <ext/nanotime.h>

__CPP_BEGIN

typedef struct sem {
    volatile atomic_int value;
    int                 signature;
    int                 reserved[2];
} sem_t;


// Initializes a semaphore object with the given number of permits.
extern int sem_init(sem_t* _Nonnull sema, int value);

// Deinitializes the given semaphore.
extern int sem_deinit(sem_t* _Nonnull sema);


// Relinquishes a permit to the semaphore.
// @Concurrency: Safe
extern int sem_post(sem_t* _Nonnull sema);

// Blocks the caller until one permit has been successfully acquired. Returns 0
// on success and -1 and on failure.
// @Concurrency: Safe
extern int sem_wait(sem_t* _Nonnull sema);

// Like sem_wait() but allows the specification of a timeout value. 'wtp' is
// an absolute point in time if 'flags' includes TIMER_ABSTIME and a duration
// relative to the current time otherwise. 
// @Concurrency: Safe
extern int sem_timedwait(sem_t* _Nonnull self, int flags, const nanotime_t* _Nonnull wtp);

// Attempts to acquire one permit without blocking. Returns 0 on success and -1
// and errno set to EAGAIN otherwise.
// @Concurrency: Safe
extern int sem_trywait(sem_t* _Nonnull sema);

__CPP_END

#endif /* _SERENA_SEM_H */
