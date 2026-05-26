//
//  rwmtx.c
//  kernel
//
//  Created by Dietmar Planitzer on 4/21/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "rwmtx.h"
#include "vcpu.h"
#include <ext/nanotime.h>
#include <assert.h>
#include <kern/kernlib.h>


// Initializes a new shared-exclusive lock.
void rwmtx_init(rwmtx_t* _Nonnull self)
{
    mtx_init(&self->mtx);
    cnd_init(&self->cv);
    self->exclusiveOwnerVpId = 0;
    self->ownerCount = 0;
    self->state = _RWMTX_UNLOCKED;
}

// Deinitializes a lock.
void rwmtx_deinit(rwmtx_t* _Nonnull self)
{
    mtx_lock(&self->mtx);
    assert(self->state == _RWMTX_UNLOCKED);
    mtx_unlock(&self->mtx);

    cnd_deinit(&self->cv);
    mtx_deinit(&self->mtx);
}

static errno_t _rwmtx_rdlock_slow(rwmtx_t* _Nonnull self)
{
    decl_try_err();

    for (;;) {
        err = cnd_wait(&self->cv, &self->mtx);
        if (err != EOK) {
            break;
        }

        if (self->state == _RWMTX_UNLOCKED || self->state == _RWMTX_LOCKED_SHARED) {
            self->state = _RWMTX_LOCKED_SHARED;
            self->ownerCount++;
            break;
        }
    }

    return err;
}

// Blocks the caller until the lock can be taken successfully in shared mode. If
// the lock was initialized with the kLockOption_InterruptibleLock option, then
// this function may be interrupted by another VP and it returns ECANCELED if this
// happens.
errno_t rwmtx_rdlock(rwmtx_t* _Nonnull self)
{
    decl_try_err();

    mtx_lock(&self->mtx);
    switch (self->state) {
        case _RWMTX_UNLOCKED:
            self->state = _RWMTX_LOCKED_SHARED;
            self->ownerCount = 1;
            break;

        case _RWMTX_LOCKED_SHARED:
            self->state = _RWMTX_LOCKED_SHARED;
            self->ownerCount++;
            break;

        case _RWMTX_LOCKED_EXCLUSIVE:
            // Someone is holding the lock in exclusive mode -> gotta wait until
            // that guy drops the exclusive lock
            err = _rwmtx_rdlock_slow(self);
            break;

        default:
            abort();
            break;
    }
    mtx_unlock(&self->mtx);

    return err;
}

static errno_t _rwmtx_wrlock_slow(rwmtx_t* _Nonnull self)
{
    decl_try_err();

    for (;;) {
        err = cnd_wait(&self->cv, &self->mtx);
        if (err != EOK) {
            break;
        }

        if (self->state == _RWMTX_UNLOCKED) {
            self->state = _RWMTX_LOCKED_EXCLUSIVE;
            self->ownerCount = 1;
            self->exclusiveOwnerVpId = vcpu_current_id();
            break;
        }
    }
    return err;
}

// Blocks the caller until the lock can be taken successfully in exclusive mode.
// If the lock was initialized with the kLockOption_InterruptibleLock option,
// then this function may be interrupted by another VP and it returns ECANCELED if
// this happens.
errno_t rwmtx_wrlock(rwmtx_t* _Nonnull self)
{
    decl_try_err();

    mtx_lock(&self->mtx);
    switch (self->state) {
        case _RWMTX_UNLOCKED:
            self->state = _RWMTX_LOCKED_EXCLUSIVE;
            self->ownerCount = 1;
            self->exclusiveOwnerVpId = vcpu_current_id();
            break;

        case _RWMTX_LOCKED_SHARED:
            _rwmtx_wrlock_slow(self);
            break;

        case _RWMTX_LOCKED_EXCLUSIVE:
            if (self->exclusiveOwnerVpId == vcpu_current_id()) {
                self->ownerCount++;
            }
            else {
                err = _rwmtx_wrlock_slow(self);
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
        case _RWMTX_LOCKED_SHARED:
            if (self->ownerCount == 1) {
                self->ownerCount = 0;
                self->state = _RWMTX_UNLOCKED;
                doBroadcast = true;
            }
            else {
                self->ownerCount--;
            }
            break;

        case _RWMTX_LOCKED_EXCLUSIVE:
            if (self->exclusiveOwnerVpId != vcpu_current_id()) {
                err = EPERM;
            }
            else {
                if (self->ownerCount == 1) {
                    self->ownerCount = 0;
                    self->exclusiveOwnerVpId = 0;
                    self->state = _RWMTX_UNLOCKED;
                    doBroadcast = true;
                }
                else {
                    self->ownerCount--;
                }
            }
            break;

        case _RWMTX_UNLOCKED:
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
