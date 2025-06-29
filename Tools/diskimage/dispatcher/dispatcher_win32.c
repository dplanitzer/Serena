//
//  dispatcher.c
//  diskimage
//
//  Created by Dietmar Planitzer on 3/10/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "Lock.h"

void Lock_Init(Lock* self)
{
    InitializeSRWLock(self);
}   

void Lock_Deinit(Lock* self)
{
}

void Lock_Lock(Lock* self)
{
    AcquireSRWLockExclusive(self);
}

void Lock_Unlock(Lock* self)
{
    ReleaseSRWLockExclusive(self);
}


////////////////////////////////////////////////////////////////////////////////

#include "ConditionVariable.h"

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

errno_t ConditionVariable_Wait(ConditionVariable* pCondVar, Lock* pLock)
{
    return (SleepConditionVariableSRW(pCondVar, pLock, INFINITE, 0) != 0) ? EOK : EINTR;
}

errno_t ConditionVariable_TimedWait(ConditionVariable* pCondVar, Lock* pLock, const struct timespec* _Nonnull deadline)
{
    return (SleepConditionVariableSRW(pCondVar, pLock, INFINITE, 0) != 0) ? EOK : EINTR;
}


////////////////////////////////////////////////////////////////////////////////

#include "SELock.h"

void SELock_Init(SELock* self)
{
    InitializeSRWLock(&self->lock);
    self->state = kSELState_Unlocked;
}

void SELock_Deinit(SELock* self)
{
}

errno_t SELock_LockShared(SELock* self)
{
    AcquireSRWLockShared(&self->lock);
    self->state = kSELState_LockedShared;
    return EOK;
}

errno_t SELock_LockExclusive(SELock* self)
{
    AcquireSRWLockExclusive(&self->lock);
    self->state = kSELState_LockedExclusive;
    return EOK;
}

errno_t SELock_Unlock(SELock* self)
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
