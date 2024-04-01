//
//  ULock.h
//  kernel
//
//  Created by Dietmar Planitzer on 3/21/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef ULock_h
#define ULock_h

#include <klib/klib.h>
#include <dispatcher/Lock.h>


OPEN_CLASS_WITH_REF(ULock, Object,
    Lock    lock;
);
typedef struct ULockMethodTable {
    ObjectMethodTable   super;
} ULockMethodTable;


// Creates a lock suitable for use by userspace code. This means that:
// - the ULock_Lock() is interruptible
extern errno_t ULock_Create(ULockRef _Nullable * _Nonnull pOutSelf);


// Attempts to acquire the given lock. True is return if the lock has been
// successfully acquired and false otherwise.
#define ULock_TryLock(__self) \
Lock_TryLock(&(__self)->lock)

// Blocks the caller until the lock can be successfully taken. This function
// may be interrupted ie if the system call that lead to this call is aborted/
// interrupted.
#define ULock_Lock(__self) \
Lock_Lock(&(__self)->lock)

// Unlocks the lock. Returns EPERM if the caller does not hold the lock and the
// lock. Otherwise returns EOK.
#define ULock_Unlock(__self) \
Lock_Unlock(&(__self)->lock)

#endif /* ULock_h */
