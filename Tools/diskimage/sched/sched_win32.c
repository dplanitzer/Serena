//
//  sched_win32.c
//  diskimage
//
//  Created by Dietmar Planitzer on 3/10/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "mtx.h"

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

#include "cnd.h"

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
    return (SleepConditionVariableSRW(pCondVar, mtx, INFINITE, 0) != 0) ? EOK : EINTR;
}

errno_t cnd_timedwait(cnd_t* pCondVar, mtx_t* mtx, const struct timespec* _Nonnull deadline)
{
    return (SleepConditionVariableSRW(pCondVar, mtx, INFINITE, 0) != 0) ? EOK : EINTR;
}


////////////////////////////////////////////////////////////////////////////////

#include "rwmtx.h"

void rwmtx_init(rwmtx_t* self)
{
    InitializeSRWLock(&self->lock);
    self->state = kSELState_Unlocked;
}

void rwmtx_deinit(rwmtx_t* self)
{
}

errno_t rwmtx_rdlock(rwmtx_t* self)
{
    AcquireSRWLockShared(&self->lock);
    self->state = kSELState_LockedShared;
    return EOK;
}

errno_t rwmtx_wrlock(rwmtx_t* self)
{
    AcquireSRWLockExclusive(&self->lock);
    self->state = kSELState_LockedExclusive;
    return EOK;
}

errno_t rwmtx_unlock(rwmtx_t* self)
{
    switch (self->state) {
        case kSELState_Unlocked:
            return EPERM;

        case kSELState_LockedShared:
            ReleaseSRWLockShared(&self->lock);
            break;

        case kSELState_LockedExclusive:
            ReleaseSRWLockExclusive(&self->lock);
            break;

        default:
            abort();
            break;
    }

    return EOK;
}
