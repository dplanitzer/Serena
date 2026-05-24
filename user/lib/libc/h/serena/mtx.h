//
//  serena/mtx.h
//  libc
//
//  Created by Dietmar Planitzer on 3/21/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef _SERENA_MTX_H
#define _SERENA_MTX_H 1

#include <_cmndef.h>
#include <ext/atomic.h>
#include <ext/errno.h>

__CPP_BEGIN

typedef struct mtx {
    volatile atomic_int state;
    int                 reserved[3];
} mtx_t;


// Initializes a mutex.
extern errno_t mtx_init(mtx_t* _Nonnull mutex);

// Deinitializes the given mutex. Triggers undefined behavior if the mutex is
// currently locked.
extern void mtx_destroy(mtx_t* _Nonnull mutex);


// Attempts to acquire the given mutex. Returns 0 on success and -1 with errno
// set to EAGAIN if the mutex is currently being held by some other execution
// context.
// @Concurrency: Safe
extern errno_t mtx_trylock(mtx_t* _Nonnull mutex);

// Blocks the caller until the mutex can be successfully taken. Returns EOK on
// success and a suitable error code if the lock operation failed for some reason.
// @Concurrency: Safe
extern errno_t mtx_lock(mtx_t* _Nonnull mutex);

// Blocks the caller until either the mutex can be successfully taken or a timeout
// has occurred. Returns EOK on success and ETIMEDOUT on a timeout.
// @Concurrency: Safe
extern errno_t mtx_timedlock(mtx_t* _Nonnull self, int flags, const nanotime_t* _Nonnull wtp);

// Unlocks the mutex. Returns EPERM if the caller does not hold the mutex.
// Returns EOK on success.
// @Concurrency: Safe
extern errno_t mtx_unlock(mtx_t* _Nonnull mutex);

__CPP_END

#endif /* _SERENA_MTX_H */
