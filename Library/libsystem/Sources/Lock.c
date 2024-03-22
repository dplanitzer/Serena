//
//  Lock.c
//  libsystem
//
//  Created by Dietmar Planitzer on 3/21/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "LockPriv.h"
#include <System/_syscall.h>


errno_t Lock_Init(LockRef _Nonnull lock)
{
    ULock* self = (ULock*)lock;

    self->signature = 0;
    self->r2 = 0;
    self->r3 = 0;

    const errno_t err = _syscall(SC_lock_create, &self->od);
    if (err == EOK) {
        self->signature = LOCK_SIGNATURE;
    }

    return err;
}

errno_t Lock_Deinit(LockRef _Nonnull lock)
{
    ULock* self = (ULock*)lock;

    if (self->signature != LOCK_SIGNATURE) {
        return EINVAL;
    }

    const errno_t err = _syscall(SC_dispose, self->od);
    self->signature = 0;
    self->od = 0;

    return err;
}

errno_t Lock_TryLock(LockRef _Nonnull lock)
{
    ULock* self = (ULock*)lock;

    if (self->signature == LOCK_SIGNATURE) {
        return _syscall(SC_lock_trylock, self->od);
    }
    else {
        return EINVAL;
    }
}

errno_t Lock_Lock(LockRef _Nonnull lock)
{
    ULock* self = (ULock*)lock;

    if (self->signature == LOCK_SIGNATURE) {
        return _syscall(SC_lock_lock, self->od);
    }
    else {
        return EINVAL;
    }
}

errno_t Lock_Unlock(LockRef _Nonnull lock)
{
    ULock* self = (ULock*)lock;

    if (self->signature == LOCK_SIGNATURE) {
        return _syscall(SC_lock_unlock, self->od);
    }
    else {
        return EINVAL;
    }
}
