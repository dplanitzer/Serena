//
//  sched_posix.c
//  diskimage
//
//  Created by Dietmar Planitzer on 1/27/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#if defined(__APPLE__)

#include <sched/mtx.h>

void mtx_init(mtx_t* self)
{
    *self = PTHREAD_MUTEX_INITIALIZER;
}   

void mtx_deinit(mtx_t* self)
{
    pthread_mutex_destroy(self);
}

void mtx_lock(mtx_t* self)
{
    pthread_mutex_lock(self);
}

void mtx_unlock(mtx_t* self)
{
    pthread_mutex_unlock(self);
}


////////////////////////////////////////////////////////////////////////////////

#include <sched/cnd.h>

void cnd_init(cnd_t* pCondVar)
{
    *pCondVar = PTHREAD_COND_INITIALIZER;
}

void cnd_deinit(cnd_t* pCondVar)
{
    pthread_cond_destroy(pCondVar);
}

void cnd_signal(cnd_t* pCondVar)
{
    pthread_signal_cond(pCondVar);
}

void cnd_broadcast(cnd_t* pCondVar)
{
    pthread_cond_broadcast(pCondVar);
}

errno_t cnd_wait(cnd_t* pCondVar, mtx_t* mtx)
{
    if (pthread_cond_wait(pCondVar, mtx) == 0) {
        return 0;
    }
    else {
        const int err = errno;
        errno = 0;
        return err;
    }
}

errno_t cnd_timedwait(cnd_t* pCondVar, mtx_t* mtx, const struct timespec* _Nonnull deadline)
{
    if (pthread_cond_timedwait(pCondVar, mtx, deadline) == 0) {
        return 0;
    }
    else {
        const int err = errno;
        errno = 0;
        return err;
    }
}


////////////////////////////////////////////////////////////////////////////////

#include <sched/rwmtx.h>

void rwmtx_init(rwmtx_t* self)
{
    *self = PTHREAD_RWLOCK_INITIALIZER;
}

void rwmtx_deinit(rwmtx_t* self)
{
    pthread_rwlock_destroy(self);
}

errno_t rwmtx_rdlock(rwmtx_t* self)
{
    if (pthread_rwlock_rdlock(self) == 0) {
        return 0;
    }
    else {
        const int err = errno;
        errno = 0;
        return err;
    }
}

errno_t rwmtx_wrlock(rwmtx_t* self)
{
    if (pthread_rwlock_wrlock(self) == 0) {
        return 0;
    }
    else {
        const int err = errno;
        errno = 0;
        return err;
    }
}

errno_t rwmtx_unlock(rwmtx_t* self)
{
    if (pthread_rwlock_unlock(self) == 0) {
        return 0;
    }
    else {
        const int err = errno;
        errno = 0;
        return err;
    }
}

#endif /* __APPLE__ */
