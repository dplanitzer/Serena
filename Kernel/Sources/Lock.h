//
//  Lock.h
//  Apollo
//
//  Created by Dietmar Planitzer on 3/27/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef Lock_h
#define Lock_h

#include "Foundation.h"
#include "List.h"


typedef struct _Lock {
    volatile UInt   value;
    List            wait_queue;
    Int             owner_vpid;     // ID of the VP that is currently holding the lock
} Lock;


// Initializes a new lock.
extern void Lock_Init(Lock*_Nonnull pLock);

// Deinitializes a lock. The lock is automatically unlocked if the calling code
// is holding the lock.
extern void Lock_Deinit(Lock* _Nonnull pLock);

// Allocates a new lock.
extern Lock* _Nullable Lock_Create(void);

// Deallocates a lock. The lock is automatically unlocked if the calling code
// is holding the lock.
extern void Lock_Destroy(Lock* _Nullable pLock);

// Blocks the caller until the lock can be taken successfully. Note that this
// function may return EINTR which means that the Lock_Lock() call is happening
// in the context of a system call that should be aborted.
extern ErrorCode Lock_Lock(Lock* _Nonnull pLock);

// Unlocks the lock.
extern void Lock_Unlock(Lock* _Nonnull pLock);

// Returns the ID of the virtual processor that is currently holding the lock.
// Zero is returned if none is holding the lock.
extern Int Lock_GetOwnerVpid(Lock* _Nonnull pLock);

#endif /* Lock_h */
