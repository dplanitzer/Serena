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
#include "_vcpu.h"

#define CV_SIGNATURE 0x53454d41


int cond_init(cond_t* _Nonnull self)
{
    self->spinlock = SPINLOCK_INIT;
    self->signature = CV_SIGNATURE;

    SList_Init(&self->wait_queue);
    sigemptyset(&self->wait_mask);
    sigaddset(&self->wait_mask, SIGCV);

    return 0;
}

int cond_deinit(cond_t* _Nonnull self)
{
    if (self->signature != CV_SIGNATURE) {
        errno = EINVAL;
        return -1;
    }

    spin_lock(&self->spinlock);
    const bool isEmpty = SList_IsEmpty(&self->wait_queue);
    spin_unlock(&self->spinlock);
    self->signature = 0;

    if (isEmpty) {
        return 0;
    }
    else {
        errno = EBUSY;
        return -1;
    }
}

int cond_signal(cond_t* _Nonnull self)
{
    SListNode* vp_node = NULL;

    if (self->signature != CV_SIGNATURE) {
        errno = EINVAL;
        return -1;
    }

    spin_lock(&self->spinlock);
    vp_node = SList_RemoveFirst(&self->wait_queue);
    spin_unlock(&self->spinlock);

    if (vp_node) {
        vcpu_t* vp = vcpu_from_wq_node(vp_node);

        sig_raise(vp->id, SIGCV);
    }
}

int cond_broadcast(cond_t* _Nonnull self)
{
    SList wq = {NULL, NULL};

    if (self->signature != CV_SIGNATURE) {
        errno = EINVAL;
        return -1;
    }

    spin_lock(&self->spinlock);
    wq = self->wait_queue;
    SList_Init(&self->wait_queue);
    spin_unlock(&self->spinlock);

    SList_ForEach(&wq, SListNode, {
        vcpu_t* vp = vcpu_from_wq_node(pCurNode);

        sig_raise(vp->id, SIGCV);
        pCurNode->next = NULL;
    });
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

    vcpu_t* vp = __vcpu_self();
    sigset_t sigs;

    spin_lock(&self->spinlock);
    SList_InsertAfterLast(&self->wait_queue, &vp->wq_node);
    spin_unlock(&self->spinlock);

    mutex_unlock(mutex);
    if (wtp) {
        sig_timedwait(&self->wait_mask, &sigs, flags, wtp);
    }
    else {
        sig_wait(&self->wait_mask, &sigs);
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
