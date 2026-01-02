//
//  cnd_wait.c
//  libc
//
//  Created by Dietmar Planitzer on 3/21/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "__synch.h"
#include <errno.h>


// We use a signalling wait queue here to ensure that after we've dropped the
// murex lock and the producer takes the mutex lock, signals and drops the mutex
// lock before we are able to enter the wait, that we don't lose the fact that
// the producer signalled us. We would miss this wakeup with a stateless wait
// queue.
static int __cnd_wait(cnd_t* _Nonnull self, mtx_t* _Nonnull mutex, int flags, const struct timespec* _Nullable wtp)
{
    if (self->signature != CND_SIGNATURE) {
        errno = EINVAL;
        return -1;
    }

    const int r = __mtx_unlock(mutex);
    if (r == 1) {
        wq_wakeup_then_timedwait(mutex->wait_queue, self->wait_queue, flags, wtp);
    }
    else if (r == 0) {
        wq_timedwait(self->wait_queue, flags, wtp);
    }
    mtx_lock(mutex);

    return (r >= 0) ? 0 : -1;
}

int cnd_wait(cnd_t* _Nonnull self, mtx_t* _Nonnull mutex)
{
    return __cnd_wait(self, mutex, TIMER_ABSTIME, &TIMESPEC_INF);
}

int cnd_timedwait(cnd_t* _Nonnull self, mtx_t* _Nonnull mutex, int flags, const struct timespec* _Nonnull wtp)
{
    return __cnd_wait(self, mutex, flags, wtp);
}
