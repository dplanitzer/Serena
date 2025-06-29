//
//  sys/condvar.c
//  libc
//
//  Created by Dietmar Planitzer on 3/21/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include <sys/condvar.h>
#include <kpi/syscall.h>
#include <errno.h>
#include <stddef.h>
#include <sys/waitqueue.h>

#define CV_SIGNATURE 0x53454d41
#define CV_SIGNAL 1


int cond_init(cond_t* _Nonnull self)
{
    self->spinlock = SPINLOCK_INIT;
    self->waiters = 0;
    self->signature = CV_SIGNATURE;
    self->wait_queue = wq_create(WAITQUEUE_FIFO);

    if (self->wait_queue >= 0) {
        return 0;
    }
    else {
        self->signature = 0;
        return -1;
    }
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

    return r;
}

static int _cond_wakeup(cond_t* _Nonnull self, int flags)
{
    bool doWakeup = false;

    if (self->signature != CV_SIGNATURE) {
        errno = EINVAL;
        return -1;
    }

    spin_lock(&self->spinlock);
    if (self->waiters > 0) {
        doWakeup = true;
    }
    spin_unlock(&self->spinlock);

    if (doWakeup) {
        wq_signal(self->wait_queue, flags, CV_SIGNAL);
    }
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

    spin_lock(&self->spinlock);
    self->waiters++;
    spin_unlock(&self->spinlock);

    mutex_unlock(mutex);
    if (wtp) {
        wq_sigtimedwait(self->wait_queue, flags, wtp, NULL);
    }
    else {
        wq_sigwait(self->wait_queue, NULL);
    }

    spin_lock(&self->spinlock);
    self->waiters--;
    spin_unlock(&self->spinlock);

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
