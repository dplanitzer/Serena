//
//  sys/condvar.c
//  libc
//
//  Created by Dietmar Planitzer on 3/21/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include <sys/condvar.h>
#include <kpi/syscall.h>
#include <sys/timespec.h>
#include <sys/waitqueue.h>
#include <errno.h>
#include <stddef.h>

extern int __mutex_unlock(mutex_t* _Nonnull self);
#define CV_SIGNATURE 0x53454d41


int cond_init(cond_t* _Nonnull self)
{
    self->spinlock = SPINLOCK_INIT;
    self->signature = CV_SIGNATURE;

    self->wait_queue = wq_create(WAITQUEUE_FIFO);
    if (self->wait_queue < 0) {
        self->signature = 0;
        return -1;
    }

    return 0;
}

int cond_deinit(cond_t* _Nonnull self)
{
    if (self->signature != CV_SIGNATURE) {
        errno = EINVAL;
        return -1;
    }

    const int r = _syscall(SC_wq_dispose, self->wait_queue);
    self->signature = 0;
    self->wait_queue = -1;

    return 0;
}

int _cond_wakeup(cond_t* _Nonnull self, int flags)
{
    if (self->signature != CV_SIGNATURE) {
        errno = EINVAL;
        return -1;
    }

    wq_wakeup(self->wait_queue, flags);
    return 0;
}

int cond_signal(cond_t* _Nonnull self)
{
    return _cond_wakeup(self, WAKE_ONE);
}

int cond_broadcast(cond_t* _Nonnull self)
{
    return _cond_wakeup(self, WAKE_ALL);
}

// We use a signalling wait queue here to ensure that after we've dropped the
// murex lock and the producer takes the mutex lock, signals and drops the mutex
// lock before we are able to enter the wait, that we don't lose the fact that
// the producer signalled us. We would miss this wakeup with a stateless wait
// queue.
static int _cond_wait(cond_t* _Nonnull self, mutex_t* _Nonnull mutex, int flags, const struct timespec* _Nullable wtp)
{
    if (self->signature != CV_SIGNATURE) {
        errno = EINVAL;
        return -1;
    }

    const int r = __mutex_unlock(mutex);
    if (r == 1) {
        wq_timedwakewait(self->wait_queue, mutex->wait_queue, NULL, flags, wtp);
    }
    else if (r == 0) {
        wq_timedwait(self->wait_queue, NULL, flags, wtp);
    }
    mutex_lock(mutex);

    return (r >= 0) ? 0 : -1;
}

int cond_wait(cond_t* _Nonnull self, mutex_t* _Nonnull mutex)
{
    return _cond_wait(self, mutex, TIMER_ABSTIME, &TIMESPEC_INF);
}

int cond_timedwait(cond_t* _Nonnull self, mutex_t* _Nonnull mutex, int flags, const struct timespec* _Nonnull wtp)
{
    return _cond_wait(self, mutex, flags, wtp);
}
