//
//  rwmtx.c
//  kernel
//
//  Created by Dietmar Planitzer on 4/21/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "rwmtx.h"
#include <dispatcher/VirtualProcessorScheduler.h>
#include <kern/timespec.h>


// Initializes a new shared-exclusive lock.
void rwmtx_init(rwmtx_t* _Nonnull self)
{
    mtx_init(&self->mtx);
    cnd_init(&self->cv);
    self->exclusiveOwnerVpId = 0;
    self->ownerCount = 0;
    self->state = kSELState_Unlocked;
}

// Deinitializes a lock.
void rwmtx_deinit(rwmtx_t* _Nonnull self)
{
    mtx_lock(&self->mtx);
    assert(self->state == kSELState_Unlocked);
    mtx_unlock(&self->mtx);

    cnd_deinit(&self->cv);
    mtx_deinit(&self->mtx);
}

static errno_t _SELock_AcquireSharedLockSlow(rwmtx_t* _Nonnull self)
{
    decl_try_err();

    for (;;) {
        err = cnd_wait(&self->cv, &self->mtx);
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
errno_t rwmtx_rdlock(rwmtx_t* _Nonnull self)
{
    decl_try_err();

    mtx_lock(&self->mtx);
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
    mtx_unlock(&self->mtx);

    return err;
}

static errno_t _SELock_AcquireExclusiveLockSlow(rwmtx_t* _Nonnull self)
{
    decl_try_err();

    for (;;) {
        err = cnd_wait(&self->cv, &self->mtx);
        if (err != EOK) {
            break;
        }

        if (self->state == kSELState_Unlocked) {
            self->state = kSELState_LockedExclusive;
            self->ownerCount = 1;
            self->exclusiveOwnerVpId = vcpu_currentid();
            break;
        }
    }
    return err;
}

// Blocks the caller until the lock can be taken successfully in exclusive mode.
// If the lock was initialized with the kLockOption_InterruptibleLock option,
// then this function may be interrupted by another VP and it returns EINTR if
// this happens.
errno_t rwmtx_wrlock(rwmtx_t* _Nonnull self)
{
    decl_try_err();

    mtx_lock(&self->mtx);
    switch (self->state) {
        case kSELState_Unlocked:
            self->state = kSELState_LockedExclusive;
            self->ownerCount = 1;
            self->exclusiveOwnerVpId = vcpu_currentid();
            break;

        case kSELState_LockedShared:
            _SELock_AcquireExclusiveLockSlow(self);
            break;

        case kSELState_LockedExclusive:
            if (self->exclusiveOwnerVpId == vcpu_currentid()) {
                self->ownerCount++;
            }
            else {
                err = _SELock_AcquireExclusiveLockSlow(self);
            }
            break;

        default:
            abort(); break;
    }
    mtx_unlock(&self->mtx);

    return EOK;
}


// Unlocks the lock.
errno_t rwmtx_unlock(rwmtx_t* _Nonnull self)
{
    decl_try_err();
    bool doBroadcast = false;

    mtx_lock(&self->mtx);
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
            if (self->exclusiveOwnerVpId != vcpu_currentid()) {
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
        cnd_broadcast(&self->cv);
    }
    mtx_unlock(&self->mtx);

    return err;
}
