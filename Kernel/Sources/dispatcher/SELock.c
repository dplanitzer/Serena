//
//  SELock.c
//  kernel
//
//  Created by Dietmar Planitzer on 4/21/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "SELock.h"
#include "VirtualProcessorScheduler.h"
#include <kern/timespec.h>


// Initializes a new shared-exclusive lock.
void SELock_Init(SELock* _Nonnull self)
{
    Lock_Init(&self->lock);
    ConditionVariable_Init(&self->cv);
    self->exclusiveOwnerVpId = 0;
    self->ownerCount = 0;
    self->state = kSELState_Unlocked;
}

// Deinitializes a lock. Returns an error and leaves the lock state unchanged if
// the lock is currently locked.
errno_t SELock_Deinit(SELock* _Nonnull self)
{
    decl_try_err();

    Lock_Lock(&self->lock);
    if (self->state != kSELState_Unlocked) {
        err = EPERM;
    }
    Lock_Unlock(&self->lock);

    if (err == EOK) {
        ConditionVariable_Deinit(&self->cv);
        Lock_Deinit(&self->lock);
    }
    return err;
}

static errno_t _SELock_AcquireSharedLockSlow(SELock* _Nonnull self)
{
    decl_try_err();

    for (;;) {
        err = ConditionVariable_Wait(&self->cv, &self->lock);
        if (err != EOK) {
            break;
        }

        if (self->state == kSELState_Unlocked || self->state == kSELState_LockedShared) {
            self->state = kSELState_LockedShared;
            self->ownerCount++;
            break;
        }
    }

    return err;
}

// Blocks the caller until the lock can be taken successfully in shared mode. If
// the lock was initialized with the kLockOption_InterruptibleLock option, then
// this function may be interrupted by another VP and it returns EINTR if this
// happens.
errno_t SELock_LockShared(SELock* _Nonnull self)
{
    decl_try_err();

    Lock_Lock(&self->lock);
    switch (self->state) {
        case kSELState_Unlocked:
            self->state = kSELState_LockedShared;
            self->ownerCount = 1;
            break;

        case kSELState_LockedShared:
            self->state = kSELState_LockedShared;
            self->ownerCount++;
            break;

        case kSELState_LockedExclusive:
            // Someone is holding the lock in exclusive mode -> gotta wait until
            // that guy drops the exclusive lock
            err = _SELock_AcquireSharedLockSlow(self);
            break;

        default:
            abort(); break;
    }
    Lock_Unlock(&self->lock);

    return err;
}

static errno_t _SELock_AcquireExclusiveLockSlow(SELock* _Nonnull self)
{
    decl_try_err();

    for (;;) {
        err = ConditionVariable_Wait(&self->cv, &self->lock);
        if (err != EOK) {
            break;
        }

        if (self->state == kSELState_Unlocked) {
            self->state = kSELState_LockedExclusive;
            self->ownerCount = 1;
            self->exclusiveOwnerVpId = VirtualProcessor_GetCurrentVpid();
            break;
        }
    }
    return err;
}

// Blocks the caller until the lock can be taken successfully in exclusive mode.
// If the lock was initialized with the kLockOption_InterruptibleLock option,
// then this function may be interrupted by another VP and it returns EINTR if
// this happens.
errno_t SELock_LockExclusive(SELock* _Nonnull self)
{
    decl_try_err();

    Lock_Lock(&self->lock);
    switch (self->state) {
        case kSELState_Unlocked:
            self->state = kSELState_LockedExclusive;
            self->ownerCount = 1;
            self->exclusiveOwnerVpId = VirtualProcessor_GetCurrentVpid();
            break;

        case kSELState_LockedShared:
            _SELock_AcquireExclusiveLockSlow(self);
            break;

        case kSELState_LockedExclusive:
            if (self->exclusiveOwnerVpId == VirtualProcessor_GetCurrentVpid()) {
                self->ownerCount++;
            }
            else {
                err = _SELock_AcquireExclusiveLockSlow(self);
            }
            break;

        default:
            abort(); break;
    }
    Lock_Unlock(&self->lock);

    return EOK;
}


// Unlocks the lock.
errno_t SELock_Unlock(SELock* _Nonnull self)
{
    decl_try_err();
    bool doBroadcast = false;

    Lock_Lock(&self->lock);
    switch (self->state) {
        case kSELState_LockedShared:
            if (self->ownerCount == 1) {
                self->ownerCount = 0;
                self->state = kSELState_Unlocked;
                doBroadcast = true;
            }
            else {
                self->ownerCount--;
            }
            break;

        case kSELState_LockedExclusive:
            if (self->exclusiveOwnerVpId != VirtualProcessor_GetCurrentVpid()) {
                err = EPERM;
            }
            else {
                if (self->ownerCount == 1) {
                    self->ownerCount = 0;
                    self->exclusiveOwnerVpId = 0;
                    self->state = kSELState_Unlocked;
                    doBroadcast = true;
                }
                else {
                    self->ownerCount--;
                }
            }
            break;

        case kSELState_Unlocked:
            err = EPERM;
            break;

        default:
            abort(); break;
    }

    if (doBroadcast) {
        ConditionVariable_BroadcastAndUnlock(&self->cv, &self->lock);
    }
    else {
        Lock_Unlock(&self->lock);
    }

    return err;
}
