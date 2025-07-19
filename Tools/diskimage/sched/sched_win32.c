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

#include <dispatcher/ConditionVariable.h>

void ConditionVariable_Init(ConditionVariable* pCondVar)
{
    InitializeConditionVariable(pCondVar);
}

void ConditionVariable_Deinit(ConditionVariable* pCondVar)
{
}

void ConditionVariable_Signal(ConditionVariable* pCondVar)
{
    WakeConditionVariable(pCondVar);
}

void ConditionVariable_Broadcast(ConditionVariable* pCondVar)
{
    WakeAllConditionVariable(pCondVar);
}

errno_t ConditionVariable_Wait(ConditionVariable* pCondVar, mtx_t* mtx)
{
    return (SleepConditionVariableSRW(pCondVar, mtx, INFINITE, 0) != 0) ? EOK : EINTR;
}

errno_t ConditionVariable_TimedWait(ConditionVariable* pCondVar, mtx_t* mtx, const struct timespec* _Nonnull deadline)
{
    return (SleepConditionVariableSRW(pCondVar, mtx, INFINITE, 0) != 0) ? EOK : EINTR;
}


////////////////////////////////////////////////////////////////////////////////

#include <dispatcher/SELock.h>

void SEmtx_init(SELock* self)
{
    InitializeSRWLock(&self->lock);
    self->state = kSELState_Unlocked;
}

void SEmtx_deinit(SELock* self)
{
}

errno_t SEmtx_lock_Shared(SELock* self)
{
    AcquireSRWLockShared(&self->lock);
    self->state = kSELState_LockedShared;
    return EOK;
}

errno_t SEmtx_lock_Exclusive(SELock* self)
{
    AcquireSRWLockExclusive(&self->lock);
    self->state = kSELState_LockedExclusive;
    return EOK;
}

errno_t SEmtx_unlock(SELock* self)
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
