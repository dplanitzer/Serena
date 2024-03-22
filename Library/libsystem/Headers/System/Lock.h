//
//  Lock.h
//  libsystem
//
//  Created by Dietmar Planitzer on 3/21/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef _SYS_LOCK_H
#define _SYS_LOCK_H 1

#include <System/_cmndef.h>
#include <System/Error.h>
#include <System/Types.h>

__CPP_BEGIN

#if !defined(__KERNEL__)

typedef struct Lock {
    char    d[16];
} Lock;
typedef Lock* LockRef;


// Initializes a lock object.
extern errno_t Lock_Init(LockRef _Nonnull lock);

// Deinitializes the given lock. Triggers undefined behavior if the lock is
// currently locked.
// XXX (joke, should be fixed to either return an error or implicitly
// unlock it. Should go with option #2).
extern errno_t Lock_Deinit(LockRef _Nonnull lock);


// Attempts to acquire the given lock. Returns EOK on success and EBUSY if the
// lock is currently being held by some other execution context.
// @Concurrency: Safe
extern errno_t Lock_TryLock(LockRef _Nonnull lock);

// Blocks the caller until the lock can be successfully taken. Returns EOK on
// success and EINVAL if the lock is not properly initialized.
// @Concurrency: Safe
extern errno_t Lock_Lock(LockRef _Nonnull lock);

// Unlocks the lock. Returns EPERM if the caller does not hold the lock. Returns
// EOK on success.
// @Concurrency: Safe
extern errno_t Lock_Unlock(LockRef _Nonnull lock);

#endif /* __KERNEL__ */

__CPP_END

#endif /* _SYS_LOCK_H */
