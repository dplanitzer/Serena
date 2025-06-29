//
//  Lock.h
//  kernel
//
//  Created by Dietmar Planitzer on 3/27/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef Lock_h
#define Lock_h

#include <dispatcher/WaitQueue.h>
#include <kern/errno.h>
#include <kern/types.h>


typedef struct Lock {
    volatile uint32_t   value;
    WaitQueue           wq;
    int                 owner_vpid;     // ID of the VP that is currently holding the lock
} Lock;


// Initializes a new lock appropriately for use in the kernel. This means that:
// - ownership tracking is turned on and violations will trigger a fatal error condition
extern void Lock_Init(Lock* _Nonnull self);

// Deinitializes a lock.
extern void Lock_Deinit(Lock* _Nonnull self);


// Attempts to acquire the given lock. True is return if the lock has been
// successfully acquired and false otherwise.
extern bool Lock_TryLock(Lock* _Nonnull self);

// Blocks the caller until the lock can be taken successfully.
extern errno_t Lock_Lock(Lock* _Nonnull self);

// Unlocks the lock. A call to fatalError() is triggered if the caller does not
// hold the lock. Otherwise returns EOK.
extern errno_t Lock_Unlock(Lock* _Nonnull self);


// Returns the ID of the virtual processor that is currently holding the lock.
// Zero is returned if none is holding the lock.
extern int Lock_GetOwnerVpid(Lock* _Nonnull self);

#endif /* Lock_h */
