//
//  sys/condvar.c
//  libc
//
//  Created by Dietmar Planitzer on 3/21/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include <sys/condvar.h>
#include <kpi/syscall.h>
#include <sys/waitqueue.h>
#include <errno.h>
#include <stddef.h>

#define CV_SIGNATURE 0x53454d41


int cond_init(cond_t* _Nonnull self)
{
    self->spinlock = SPINLOCK_INIT;
    self->signature = CV_SIGNATURE;

    sigemptyset(&self->wait_mask);
    sigaddset(&self->wait_mask, SIGSYNCH);

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

    const int r = _syscall(SC_dispose, self->wait_queue);
    self->signature = 0;
    self->wait_queue = -1;
}

int cond_signal(cond_t* _Nonnull self)
{
    if (self->signature != CV_SIGNATURE) {
        errno = EINVAL;
        return -1;
    }

    wq_wakeup(self->wait_queue, WAKE_ONE, SIGSYNCH);
}

int cond_broadcast(cond_t* _Nonnull self)
{
    if (self->signature != CV_SIGNATURE) {
        errno = EINVAL;
        return -1;
    }

    wq_wakeup(self->wait_queue, WAKE_ALL, SIGSYNCH);
}

// We use a signalling wait queue here to ensure that after we've dropped the
// murex lock and the producer takes the mutex lock, signals and drops the mutex
// lock before we are able to enter the wait, that we don't lose the fact that
// the producer signalled us. We would miss this wakeup with a stateless wait
// queue.
static int _cond_wait(cond_t* _Nonnull self, mutex_t* _Nonnull mutex, int flags, const struct timespec* _Nullable wtp)
{
    sigset_t sigs;

    if (self->signature != CV_SIGNATURE) {
        errno = EINVAL;
        return -1;
    }

    mutex_unlock(mutex);
    if (wtp) {
        wq_sigtimedwait(self->wait_queue, &self->wait_mask, &sigs, flags, wtp);
    }
    else {
        wq_sigwait(self->wait_queue, &self->wait_mask, &sigs);
    }

    mutex_lock(mutex);
}

int cond_wait(cond_t* _Nonnull self, mutex_t* _Nonnull mutex)
{
    return _cond_wait(self, mutex, 0, NULL);
}

int cond_timedwait(cond_t* _Nonnull self, mutex_t* _Nonnull mutex, int flags, const struct timespec* _Nonnull wtp)
{
    return _cond_wait(self, mutex, flags, wtp);
}
