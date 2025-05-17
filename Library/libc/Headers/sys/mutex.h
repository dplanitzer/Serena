//
//  sys/mutex.h
//  libc
//
//  Created by Dietmar Planitzer on 3/21/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef _SYS_MUTEX_H
#define _SYS_MUTEX_H 1

#include <_cmndef.h>
#include <sys/errno.h>

__CPP_BEGIN

typedef struct mutex {
    int d[4];
} mutex_t;


// Initializes a mutex.
extern errno_t mutex_init(mutex_t* _Nonnull mutex);

// Deinitializes the given mutex. Triggers undefined behavior if the mutex is
// currently locked.
// XXX (joke, should be fixed to either return an error or implicitly
// unlock it. Should go with option #2).
extern errno_t mutex_deinit(mutex_t* _Nonnull mutex);


// Attempts to acquire the given mutex. Returns EOK on success and EBUSY if the
// mutex is currently being held by some other execution context.
// @Concurrency: Safe
extern errno_t mutex_trylock(mutex_t* _Nonnull mutex);

// Blocks the caller until the mutex can be successfully taken. Returns EOK on
// success and EINVAL if the mutex is not properly initialized.
// @Concurrency: Safe
extern errno_t mutex_lock(mutex_t* _Nonnull mutex);

// Unlocks the mutex. Returns EPERM if the caller does not hold the mutex.
// Returns EOK on success.
// @Concurrency: Safe
extern errno_t mutex_unlock(mutex_t* _Nonnull mutex);

__CPP_END

#endif /* _SYS_MUTEX_H */
