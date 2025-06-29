//
//  sys/mutex.c
//  libc
//
//  Created by Dietmar Planitzer on 6/26/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <errno.h>
#include <stdbool.h>
#include <kpi/syscall.h>
#include <sys/mutex.h>
#include <sys/waitqueue.h>

#define MUTEX_SIGNATURE 0x4c4f434b


int mutex_init(mutex_t* _Nonnull self)
{
    self->spinlock = SPINLOCK_INIT;
    self->state = 0;
    self->waiters = 0;
    self->signature = MUTEX_SIGNATURE;
    self->wait_queue = wq_create(WAITQUEUE_FIFO);

    if (self->wait_queue >= 0) {
        return 0;
    }
    else {
        self->signature = 0;
        return -1;
    }
}

int mutex_deinit(mutex_t* _Nonnull self)
{
    if (self->signature != MUTEX_SIGNATURE) {
        errno = EINVAL;
        return -1;
    }

    const int r = _syscall(SC_dispose, self->wait_queue);
    self->signature = 0;
    self->wait_queue = -1;

    return r;
}

int mutex_trylock(mutex_t* _Nonnull self)
{
    if (self->signature != MUTEX_SIGNATURE) {
        errno = EINVAL;
        return -1;
    }

    spin_lock(&self->spinlock);
    if (self->state == 0) {
        self->state = 1;
        spin_unlock(&self->spinlock);
        return 0;
    }
    spin_unlock(&self->spinlock);

    errno = EBUSY;
    return -1;
}

int mutex_lock(mutex_t* _Nonnull self)
{
    bool didWakeup = false;

    if (self->signature != MUTEX_SIGNATURE) {
        errno = EINVAL;
        return -1;
    }

    for (;;) {
        spin_lock(&self->spinlock);
        if (didWakeup) {
            self->waiters--;
        }

        if (self->state == 0) {
            self->state = 1;
            spin_unlock(&self->spinlock);
            return 0;
        }

        self->waiters++;
        spin_unlock(&self->spinlock);
        wq_wait(self->wait_queue);
        didWakeup = true;
    }
}

int mutex_unlock(mutex_t* _Nonnull self)
{
    bool doWakeup = false;

    if (self->signature != MUTEX_SIGNATURE) {
        errno = EINVAL;
        return -1;
    }

    spin_lock(&self->spinlock);
    self->state = 0;

    if (self->waiters > 0) {
        doWakeup = true;
    }
    spin_unlock(&self->spinlock);

    if (doWakeup) {
        wq_wakeup(self->wait_queue, WAKE_ONE);
    }
}
