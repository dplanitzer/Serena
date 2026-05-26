//
//  sched_win32.c
//  diskimage
//
//  Created by Dietmar Planitzer on 3/10/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#if defined(_WIN32) || defined(_WIN64)

#include <sched/mtx.h>

void mtx_init(mtx_t* self)
{
    InitializeSRWLock(self);
}   

void mtx_deinit(mtx_t* self)
{
}

void mtx_lock(mtx_t* self)
{
    AcquireSRWLockExclusive(self);
}

void mtx_unlock(mtx_t* self)
{
    ReleaseSRWLockExclusive(self);
}


////////////////////////////////////////////////////////////////////////////////

#include <sched/cnd.h>

void cnd_init(cnd_t* pCondVar)
{
    InitializeConditionVariable(pCondVar);
}

void cnd_deinit(cnd_t* pCondVar)
{
}

void cnd_signal(cnd_t* pCondVar)
{
    WakeConditionVariable(pCondVar);
}

void cnd_broadcast(cnd_t* pCondVar)
{
    WakeAllConditionVariable(pCondVar);
}

errno_t cnd_wait(cnd_t* pCondVar, mtx_t* mtx)
{
    return (SleepConditionVariableSRW(pCondVar, mtx, INFINITE, 0) != 0) ? EOK : ECANCELED;
}

errno_t cnd_timedwait(cnd_t* pCondVar, mtx_t* mtx, const nanotime_t* _Nonnull deadline)
{
    return (SleepConditionVariableSRW(pCondVar, mtx, INFINITE, 0) != 0) ? EOK : ECANCELED;
}


////////////////////////////////////////////////////////////////////////////////

#include <sched/rwmtx.h>

void rwmtx_init(rwmtx_t* self)
{
    InitializeSRWLock(&self->lock);
    self->state = _RWMTX_UNLOCKED;
}

void rwmtx_deinit(rwmtx_t* self)
{
}

errno_t rwmtx_rdlock(rwmtx_t* self)
{
    AcquireSRWLockShared(&self->lock);
    self->state = _RWMTX_LOCKED_SHARED;
    return EOK;
}

errno_t rwmtx_wrlock(rwmtx_t* self)
{
    AcquireSRWLockExclusive(&self->lock);
    self->state = _RWMTX_LOCKED_EXCLUSIVE;
    return EOK;
}

errno_t rwmtx_unlock(rwmtx_t* self)
{
    switch (self->state) {
        case _RWMTX_UNLOCKED:
            return EPERM;

        case _RWMTX_LOCKED_SHARED:
            ReleaseSRWLockShared(&self->lock);
            break;

        case _RWMTX_LOCKED_EXCLUSIVE:
            ReleaseSRWLockExclusive(&self->lock);
            break;

        default:
            abort();
            break;
    }

    return EOK;
}

#endif /* _WIN32 */
