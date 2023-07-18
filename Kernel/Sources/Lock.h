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
//
// Inside the kernel there are two different contexts in which we might want to
// call Lock_Lock():
//
// 1) outside a system call context
// 2) inside a system call context
//
// In case (1) the correct way to call this function is to write it like this:
//
// try_bang(Lock_Lock(l));
//
// This will halt the machine if the lock fails with an EINTR because receiving
// an EINTR in this context is unexpected and points to a design error. Examples
// of this kind of context is code that runs at boot time or is executed by the
// kernel main dispatch queue or any other kind of dispatch queue created by the
// kernel for exclusive use by kernel code.
//
// In case (2) the correct way to call this function is to write it like this:
//
// try(Lock_Lock(l));
//
// This will branch to a 'catch' label which is where you should write code to
// clean up the local stack frame before you return with an error. The goal in
// in this case is to unwind the system call with an EINTR error as fast as
// possible. Once the kernel tries to return from the system call to user space,
// the kernel execution flow will be redirected to abort the original call-as-user
// invocation. This will then land the kernel back in the heart of the dispatch
// queue run function.
extern ErrorCode Lock_Lock(Lock* _Nonnull pLock);

// Unlocks the lock.
extern void Lock_Unlock(Lock* _Nonnull pLock);

// Returns the ID of the virtual processor that is currently holding the lock.
// Zero is returned if none is holding the lock.
extern Int Lock_GetOwnerVpid(Lock* _Nonnull pLock);

#endif /* Lock_h */
