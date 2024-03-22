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
#include <System/Types.h>
#include <System/TimeInterval.h>

__CPP_BEGIN

#if !defined(__KERNEL__)

typedef struct Semaphore {
    char    d[16];
} Semaphore;
typedef Semaphore* SemaphoreRef;


// Initializes a semaphore object with the given number of permits.
extern errno_t Semaphore_Init(SemaphoreRef _Nonnull sema, int npermits);

// Deinitializes the given semaphore.
extern errno_t Semaphore_Deinit(SemaphoreRef _Nonnull sema);


// Relinquishes the given number of permits to the given semaphore. This makes
// the permits available for acquisition by other execution contexts.
// @Concurrency: Safe
extern errno_t Semaphore_Relinquish(SemaphoreRef _Nonnull sema, int npermits);

// Blocks the caller until 'npermits' can be acquired. Returns EOK on success
// and ETIMEOUT if the permits could not be acquire before 'deadline'.
// @Concurrency: Safe
extern errno_t Semaphore_Acquire(SemaphoreRef _Nonnull sema, int npermits, TimeInterval deadline);

// Attempts to acquire 'npermits' without blocking. Returns EOK on success and
// EBUSY on failure.
// @Concurrency: Safe
extern errno_t Semaphore_TryAcquire(SemaphoreRef _Nonnull sema, int npermits);

#endif /* __KERNEL__ */

__CPP_END

#endif /* _SYS_SEMAPHORE_H */
