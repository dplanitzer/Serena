//
//  Lock.h
//  kernel
//
//  Created by Dietmar Planitzer on 3/27/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef Lock_h
#define Lock_h

#include <kern/errno.h>
#include <kern/types.h>
#include <klib/List.h>


typedef struct Lock {
    volatile uint32_t   value;
    List                wait_queue;
    int                 owner_vpid;     // ID of the VP that is currently holding the lock
    uint32_t            options;
} Lock;


// Lock initialization options
enum {
    kLockOption_FatalOwnershipViolations = 1,   // Ownership violations trigger a call to fatalError()
    kLockOption_InterruptibleLock = 2,          // Marks Lock_Lock() as interruptible, which means that it may return with an EINTR error instead of claiming the lock
};


// Initializes a new lock appropriately for use in the kernel. This means that:
// - ownership tracking is turned on and violations will trigger a fatal error condition
extern void Lock_Init(Lock* _Nonnull self);

// Initializes a new lock with options.
extern void Lock_InitWithOptions(Lock*_Nonnull self, uint32_t options);

// Deinitializes a lock. The lock is automatically unlocked if the calling code
// is holding the lock. Returns EPERM if non-fatal ownership validation is
// enabled, the lock is currently being held and the caller is not the owner of
// the lock. Otherwise EOK is returned.
extern errno_t Lock_Deinit(Lock* _Nonnull self);


// Attempts to acquire the given lock. True is return if the lock has been
// successfully acquired and false otherwise.
extern bool Lock_TryLock(Lock* _Nonnull self);

// Blocks the caller until the lock can be taken successfully. If the lock was
// initialized with the kLockOption_InterruptibleLock option, then this function
// may be interrupted by another VP and it returns EINTR if this happens.
extern errno_t Lock_Lock(Lock* _Nonnull self);

// Unlocks the lock. Returns EPERM if the caller does not hold the lock and the
// lock was not initialized with the kLockOption_FatalOwnershipViolations option.
// A call to fatalError() is triggered if fatal ownership violation checks are
// enabled and the caller does not hold the lock. Otherwise returns EOK.
extern errno_t Lock_Unlock(Lock* _Nonnull self);


// Returns the ID of the virtual processor that is currently holding the lock.
// Zero is returned if none is holding the lock.
extern int Lock_GetOwnerVpid(Lock* _Nonnull self);

#endif /* Lock_h */
