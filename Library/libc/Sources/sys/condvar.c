//
//  sys/condvar.c
//  libc
//
//  Created by Dietmar Planitzer on 3/21/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include <sys/condvar.h>
#include <sys/_syscall.h>
#include <errno.h>
#include <stddef.h>
#include "_mutex.h"

#define CV_SIGNATURE 0x53454d41

// Must be sizeof(UConditionVariable) <= 16 
typedef struct UConditionVariable {
    int             od;
    unsigned int    signature;
    int             r2;
    int             r3;
} UConditionVariable;


int cond_init(cond_t* _Nonnull cv)
{
    UConditionVariable* self = (UConditionVariable*)cv;

    self->signature = 0;
    self->r2 = 0;
    self->r3 = 0;

    if (_syscall(SC_cond_create, &self->od) == 0) {
        self->signature = CV_SIGNATURE;
        return 0;
    }
    else {
        return -1;
    }
}

int cond_deinit(cond_t* _Nonnull cv)
{
    UConditionVariable* self = (UConditionVariable*)cv;

    if (self->signature != CV_SIGNATURE) {
        errno = EINVAL;
        return -1;
    }

    const int r = _syscall(SC_dispose, self->od);
    self->signature = 0;
    self->od = 0;

    return r;
}

int cond_signal(cond_t* _Nonnull cv, mutex_t* _Nullable mutex)
{
    UConditionVariable* self = (UConditionVariable*)cv;
    UMutex* ulock = (UMutex*)mutex;

    if (self->signature == CV_SIGNATURE && ulock->signature == MUTEX_SIGNATURE) {
        return _syscall(SC_cond_wake, self->od, ulock->od, 0);
    }
    else {
        errno = EINVAL;
        return -1;
    }
}

int cond_broadcast(cond_t* _Nonnull cv, mutex_t* _Nullable mutex)
{
    UConditionVariable* self = (UConditionVariable*)cv;
    UMutex* ulock = (UMutex*)mutex;

    if (self->signature == CV_SIGNATURE && ulock->signature == MUTEX_SIGNATURE) {
        return _syscall(SC_cond_wake, self->od, ulock->od, 1);
    }
    else {
        errno = EINVAL;
        return -1;
    }
}

int cond_wait(cond_t* _Nonnull cv, mutex_t* _Nonnull mutex)
{
    UConditionVariable* self = (UConditionVariable*)cv;
    UMutex* ulock = (UMutex*)mutex;

    if (self->signature == CV_SIGNATURE && ulock->signature == MUTEX_SIGNATURE) {
        return _syscall(SC_cond_timedwait, self->od, ulock->od, NULL);
    }
    else {
        errno = EINVAL;
        return -1;
    }
}

int cond_timedwait(cond_t* _Nonnull cv, mutex_t* _Nonnull mutex, const struct timespec* _Nonnull deadline)
{
    UConditionVariable* self = (UConditionVariable*)cv;
    UMutex* ulock = (UMutex*)mutex;

    if (self->signature == CV_SIGNATURE && ulock->signature == MUTEX_SIGNATURE) {
        return _syscall(SC_cond_timedwait, self->od, ulock->od, deadline);
    }
    else {
        errno = EINVAL;
        return -1;
    }
}
