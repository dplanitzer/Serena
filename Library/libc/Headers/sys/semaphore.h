//
//  Semaphore.h
//  libsystem
//
//  Created by Dietmar Planitzer on 3/21/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef _SYS_SEMAPHORE_H
#define _SYS_SEMAPHORE_H 1

#include <System/_cmndef.h>
#include <System/Error.h>
#include <System/TimeInterval.h>

__CPP_BEGIN

typedef struct sem {
    int d[4];
} sem_t;


// Initializes a semaphore object with the given number of permits.
extern errno_t sem_init(sem_t* _Nonnull sema, int npermits);

// Deinitializes the given semaphore.
extern errno_t sem_deinit(sem_t* _Nonnull sema);


// Relinquishes the given number of permits to the given semaphore. This makes
// the permits available for acquisition by other execution contexts.
// @Concurrency: Safe
extern errno_t sem_post(sem_t* _Nonnull sema, int npermits);

// Blocks the caller until 'npermits' can be acquired. Returns EOK on success
// and ETIMEOUT if the permits could not be acquire before 'deadline'.
// @Concurrency: Safe
extern errno_t sem_wait(sem_t* _Nonnull sema, int npermits, TimeInterval deadline);

// Attempts to acquire 'npermits' without blocking. Returns EOK on success and
// EBUSY on failure.
// @Concurrency: Safe
extern errno_t sem_trywait(sem_t* _Nonnull sema, int npermits);

__CPP_END

#endif /* _SYS_SEMAPHORE_H */
