//
//  sys/mtx.c
//  libc
//
//  Created by Dietmar Planitzer on 6/26/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <errno.h>
#include <stdbool.h>
#include <kpi/syscall.h>
#include <sys/mtx.h>
#include <sys/waitqueue.h>

#define MTX_SIGNATURE 0x4c4f434b


int mtx_init(mtx_t* _Nonnull self)
{
    self->spinlock = SPINLOCK_INIT;
    self->state = 0;
    self->waiters = 0;
    self->signature = MTX_SIGNATURE;
    self->wait_queue = wq_create(WAITQUEUE_FIFO);

    if (self->wait_queue >= 0) {
        return 0;
    }
    else {
        self->signature = 0;
        return -1;
    }
}

int mtx_deinit(mtx_t* _Nonnull self)
{
    if (self->signature != MTX_SIGNATURE) {
        errno = EINVAL;
        return -1;
    }

    const int r = _syscall(SC_wq_dispose, self->wait_queue);
    self->signature = 0;
    self->wait_queue = -1;

    return r;
}

int mtx_trylock(mtx_t* _Nonnull self)
{
    if (self->signature != MTX_SIGNATURE) {
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

int mtx_lock(mtx_t* _Nonnull self)
{
    bool didWakeup = false;

    if (self->signature != MTX_SIGNATURE) {
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
        wq_wait(self->wait_queue, NULL);
        didWakeup = true;
    }
    /* NOT REACHED */
}

int __mtx_unlock(mtx_t* _Nonnull self)
{
    bool doWakeup = false;

    if (self->signature != MTX_SIGNATURE) {
        errno = EINVAL;
        return -1;
    }

    spin_lock(&self->spinlock);
    self->state = 0;

    if (self->waiters > 0) {
        doWakeup = true;
    }
    spin_unlock(&self->spinlock);

    return (doWakeup) ? 1 : 0;
}

int mtx_unlock(mtx_t* _Nonnull self)
{
    const int r = __mtx_unlock(self);

    if (r == 1) {
        wq_wakeup(self->wait_queue, WAKE_ONE);
        return 0;
    }
    else {
        return r;
    }
}
