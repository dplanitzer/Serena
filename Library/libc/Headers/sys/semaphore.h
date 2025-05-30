//
//  Semaphore.h
//  libc
//
//  Created by Dietmar Planitzer on 3/21/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef _SYS_SEMAPHORE_H
#define _SYS_SEMAPHORE_H 1

#include <_cmndef.h>
#include <time.h>

__CPP_BEGIN

typedef struct sem {
    int d[4];
} sem_t;


// Initializes a semaphore object with the given number of permits.
extern int sem_init(sem_t* _Nonnull sema, int npermits);

// Deinitializes the given semaphore.
extern int sem_deinit(sem_t* _Nonnull sema);


// Relinquishes the given number of permits to the given semaphore. This makes
// the permits available for acquisition by other execution contexts.
// @Concurrency: Safe
extern int sem_post(sem_t* _Nonnull sema, int npermits);

// Blocks the caller until 'npermits' can be acquired. Returns 0 on success
// and -1 and errno set to ETIMEOUT if the permits could not be acquire before
// 'deadline'.
// @Concurrency: Safe
extern int sem_wait(sem_t* _Nonnull sema, int npermits, struct timespec deadline);

// Attempts to acquire 'npermits' without blocking. Returns 0 on success and -1
// and errno set to EBUSY otherwise.
// @Concurrency: Safe
extern int sem_trywait(sem_t* _Nonnull sema, int npermits);

__CPP_END

#endif /* _SYS_SEMAPHORE_H */
