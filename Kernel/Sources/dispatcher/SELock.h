//
//  SELock.h
//  kernel
//
//  Created by Dietmar Planitzer on 4/21/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef SELock_h
#define SELock_h

#include "ConditionVariable.h"
#include <sched/mtx.h>

enum {
    kSELState_Unlocked = 0,
    kSELState_LockedShared,
    kSELState_LockedExclusive
};


// A shared-exclusive lock offers two modes of locking:
// - shared: multiple VPs may lock the lock in shared mode at the same time.
// - exclusive: at most one VP may hold the lock in exclusive mode.
// If a VP wants to take a shared-mode lock and the lock is currently in shared
// mode or unlocked then the lock request will be granted immediately. If however
// the lock is currently in exclusive mode then the lock requestor will have to
// wait until the lock owner unlocks the lock.
// If a VP wants to take an exclusive-mode lock and the lock is currently unlocked
// then the request will be granted immediately. If however the lock is currently
// in shared or exclusive mode then the requestor will have to wait until the
// single exclusive owner has unlocked the lock or all shared-mode lock owners
// have unlocked the lock.
//
// Note that a shared-exclusive lock is always interruptable. This is different
// from simple kernel-mode locks which are not interruptible.
//
// Note that a single shared or exclusive lock owner is allowed to take the lock
// multiple times (aka recursive locking). However the lock does not currently
// track the identity of shared-mode lock owners. So it's important to follow
// the locking protocol exactly to avoid problems.
typedef struct SELock {
    mtx_t               mtx;    // The management lock is not interruptible since it protects a very short code sequence and this keeps things simpler
    ConditionVariable   cv;     // The CV is always interruptible
    int                 exclusiveOwnerVpId;     // ID of the VP that is holding the lock in exclusive-mode; 0 if unlocked or locked in shared-mode
    int32_t             ownerCount;             // Count of shared-mode lock owners or recursion count of the exclusive-mode lock owner
    int32_t             state;
} SELock;


// Initializes a new shared-exclusive lock.
extern void SEmtx_init(SELock* _Nonnull self);

// Deinitializes a lock.
extern void SEmtx_deinit(SELock* _Nonnull self);


// Blocks the caller until the lock can be taken successfully in shared mode. If
// the lock was initialized with the kLockOption_InterruptibleLock option, then
// this function may be interrupted by another VP and it returns EINTR if this
// happens. It is permissible for a virtual processor to take a shared lock
// multiple times.
extern errno_t SEmtx_lock_Shared(SELock* _Nonnull self);

// Blocks the caller until the lock can be taken successfully in exclusive mode.
// If the lock was initialized with the kLockOption_InterruptibleLock option,
// then this function may be interrupted by another VP and it returns EINTR if
// this happens. A virtual processor may take a lock exclusively multiple times.
extern errno_t SEmtx_lock_Exclusive(SELock* _Nonnull self);

// Unlocks the lock. Returns EPERM if the caller does not hold the lock and the
// lock was not initialized with the kLockOption_FatalOwnershipViolations option.
// A call to fatalError() is triggered if fatal ownership violation checks are
// enabled and the caller does not hold the lock. Otherwise returns EOK.
extern errno_t SEmtx_unlock(SELock* _Nonnull self);

#endif /* SELock_h */
