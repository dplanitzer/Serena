//
//  Lock.h
//  Apollo
//
//  Created by Dietmar Planitzer on 3/27/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef Lock_h
#define Lock_h

#include "ULock.h"


typedef ULock Lock;


// Initializes a new lock.
static inline void Lock_Init(Lock* _Nonnull pLock) {
    ULock_Init(pLock);
}

// Deinitializes a lock. The lock is automatically unlocked if the calling code
// is holding the lock.
extern void Lock_Deinit(Lock* _Nonnull pLock);

// Attempts to acquire the given lock. True is return if teh lock has been
// successfully acquired and false otherwise.
static inline Bool Lock_TryLock(Lock* _Nonnull pLock) {
    return ULock_TryLock(pLock) == EOK;
}

// Blocks the caller until the lock can be taken successfully. Note that a kernel
// level lock attempt will always block until the lock can be acquired. These
// kind of locks are not interruptable.
extern void Lock_Lock(Lock* _Nonnull pLock);

// Unlocks the lock.
extern void Lock_Unlock(Lock* _Nonnull pLock);

// Returns the ID of the virtual processor that is currently holding the lock.
// Zero is returned if none is holding the lock.
static inline Int Lock_GetOwnerVpid(Lock* _Nonnull pLock) {
    return ULock_GetOwnerVpid(pLock);
}

#endif /* Lock_h */
