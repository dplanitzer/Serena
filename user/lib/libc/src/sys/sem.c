//
//  sys/sem.c
//  libc
//
//  Created by Dietmar Planitzer on 3/21/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include <sys/sem.h>
#include <errno.h>
#include <kpi/syscall.h>
#include <sys/waitqueue.h>

#define SEM_SIGNATURE 0x53454d41


int sem_init(sem_t* _Nonnull self, int npermits)
{
    self->spinlock = SPINLOCK_INIT;
    self->permits = npermits;
    self->waiters = 0;
    self->signature = SEM_SIGNATURE;
    self->wait_queue = wq_create(WAITQUEUE_FIFO);

    if (self->wait_queue >= 0) {
        return 0;
    }
    else {
        self->signature = 0;
        return -1;
    }
}

int sem_deinit(sem_t* _Nonnull self)
{
    if (self->signature != SEM_SIGNATURE) {
        errno = EINVAL;
        return -1;
    }

    const int r = _syscall(SC_wq_dispose, self->wait_queue);
    self->signature = 0;
    self->wait_queue = -1;

    return r;
}

int sem_post(sem_t* _Nonnull self, int npermits)
{
    bool doWakeup = false;

    if (self->signature != SEM_SIGNATURE) {
        errno = EINVAL;
        return -1;
    }

    if (npermits <= 0) {
        errno = ERANGE;
        return -1;
    }

    spin_lock(&self->spinlock);
    self->permits += npermits;

    if (self->waiters > 0) {
        doWakeup = true;
    }
    spin_unlock(&self->spinlock);

    if (doWakeup) {
        wq_wakeup(self->wait_queue, WAKE_ONE);
    }

    return 0;
}

int sem_wait(sem_t* _Nonnull self, int npermits)
{
    bool didWakeup = false;

    if (self->signature != SEM_SIGNATURE) {
        errno = EINVAL;
        return -1;
    }

    if (npermits <= 0) {
        errno = ERANGE;
        return -1;
    }

    for (;;) {
        spin_lock(&self->spinlock);
        if (didWakeup) {
            self->waiters--;
        }

        const int take_permits = (self->permits >= npermits) ? npermits : self->permits;

        self->permits -= take_permits;
        npermits -= take_permits;
        if (npermits == 0) {
            spin_unlock(&self->spinlock);
            return 0;
        }

        self->waiters++;
        spin_unlock(&self->spinlock);
        wq_wait(self->wait_queue);
        didWakeup = true;
    }
    /* NOT REACHED */
}

int sem_timedwait(sem_t* _Nonnull self, int npermits, int flags, const struct timespec* _Nonnull wtp)
{
    bool didWakeup = false;

    if (self->signature != SEM_SIGNATURE) {
        errno = EINVAL;
        return -1;
    }

    if (npermits <= 0) {
        errno = ERANGE;
        return -1;
    }

    for (;;) {
        spin_lock(&self->spinlock);
        if (didWakeup) {
            self->waiters--;
        }

        const int take_permits = (self->permits >= npermits) ? npermits : self->permits;

        self->permits -= take_permits;
        npermits -= take_permits;
        if (npermits == 0) {
            spin_unlock(&self->spinlock);
            return 0;
        }

        self->waiters++;
        spin_unlock(&self->spinlock);
        wq_timedwait(self->wait_queue, flags, wtp);
        didWakeup = true;
    }
    /* NOT REACHED */
}

int sem_trywait(sem_t* _Nonnull self, int npermits)
{
    if (self->signature != SEM_SIGNATURE) {
        errno = EINVAL;
        return -1;
    }

    if (npermits <= 0) {
        errno = ERANGE;
        return -1;
    }

    spin_lock(&self->spinlock);
    if (self->permits >= npermits) {
        self->permits -= npermits;
        spin_unlock(&self->spinlock);
        return 0;
    }
    spin_unlock(&self->spinlock);

    errno = EBUSY;
    return -1;
}
