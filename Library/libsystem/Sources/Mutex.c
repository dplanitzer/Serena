//
//  Mutex.c
//  libsystem
//
//  Created by Dietmar Planitzer on 3/21/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "MutexPriv.h"
#include <System/_syscall.h>


errno_t mutex_init(mutex_t* _Nonnull mutex)
{
    UMutex* self = (UMutex*)mutex;

    self->signature = 0;
    self->r2 = 0;
    self->r3 = 0;

    const errno_t err = _syscall(SC_lock_create, &self->od);
    if (err == EOK) {
        self->signature = MUTEX_SIGNATURE;
    }

    return err;
}

errno_t mutex_deinit(mutex_t* _Nonnull mutex)
{
    UMutex* self = (UMutex*)mutex;

    if (self->signature != MUTEX_SIGNATURE) {
        return EINVAL;
    }

    const errno_t err = _syscall(SC_dispose, self->od);
    self->signature = 0;
    self->od = 0;

    return err;
}

errno_t mutex_trylock(mutex_t* _Nonnull mutex)
{
    UMutex* self = (UMutex*)mutex;

    if (self->signature == MUTEX_SIGNATURE) {
        return _syscall(SC_lock_trylock, self->od);
    }
    else {
        return EINVAL;
    }
}

errno_t mutex_lock(mutex_t* _Nonnull mutex)
{
    UMutex* self = (UMutex*)mutex;

    if (self->signature == MUTEX_SIGNATURE) {
        return _syscall(SC_lock_lock, self->od);
    }
    else {
        return EINVAL;
    }
}

errno_t mutex_unlock(mutex_t* _Nonnull mutex)
{
    UMutex* self = (UMutex*)mutex;

    if (self->signature == MUTEX_SIGNATURE) {
        return _syscall(SC_lock_unlock, self->od);
    }
    else {
        return EINVAL;
    }
}
