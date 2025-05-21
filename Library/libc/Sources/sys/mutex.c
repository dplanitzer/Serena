//
//  sys/mutex.c
//  libc
//
//  Created by Dietmar Planitzer on 3/21/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "_mutex.h"
#include <errno.h>
#include <sys/_syscall.h>


int mutex_init(mutex_t* _Nonnull mutex)
{
    UMutex* self = (UMutex*)mutex;

    self->signature = 0;
    self->r2 = 0;
    self->r3 = 0;

    if (_syscall(SC_lock_create, &self->od) == 0) {
        self->signature = MUTEX_SIGNATURE;
        return 0;
    }
    else {
        return -1;
    }
}

int mutex_deinit(mutex_t* _Nonnull mutex)
{
    UMutex* self = (UMutex*)mutex;

    if (self->signature != MUTEX_SIGNATURE) {
        errno = EINVAL;
        return -1;
    }

    const int r = _syscall(SC_dispose, self->od);
    self->signature = 0;
    self->od = 0;

    return r;
}

int mutex_trylock(mutex_t* _Nonnull mutex)
{
    UMutex* self = (UMutex*)mutex;

    if (self->signature == MUTEX_SIGNATURE) {
        return _syscall(SC_lock_trylock, self->od);
    }
    else {
        errno = EINVAL;
        return -1;
    }
}

int mutex_lock(mutex_t* _Nonnull mutex)
{
    UMutex* self = (UMutex*)mutex;

    if (self->signature == MUTEX_SIGNATURE) {
        return _syscall(SC_lock_lock, self->od);
    }
    else {
        errno = EINVAL;
        return -1;
    }
}

int mutex_unlock(mutex_t* _Nonnull mutex)
{
    UMutex* self = (UMutex*)mutex;

    if (self->signature == MUTEX_SIGNATURE) {
        return _syscall(SC_lock_unlock, self->od);
    }
    else {
        errno = EINVAL;
        return -1;
    }
}
