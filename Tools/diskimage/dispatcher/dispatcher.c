//
//  dispatcher.c
//  diskimage
//
//  Created by Dietmar Planitzer on 3/10/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "Lock.h"

void Lock_Init(Lock* pLock)
{
    InitializeSRWLock(pLock);
}   

void Lock_Deinit(Lock* pLock)
{
}

void Lock_Lock(Lock* pLock)
{
    AcquireSRWLockExclusive(pLock);
}

void Lock_Unlock(Lock* pLock)
{
    ReleaseSRWLockExclusive(pLock);
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

void ConditionVariable_BroadcastAndUnlock(ConditionVariable* pCondVar, Lock* pLock)
{
    WakeAllConditionVariable(pCondVar);
    ReleaseSRWLockExclusive(pLock);
}

errno_t ConditionVariable_Wait(ConditionVariable* pCondVar, Lock* pLock, TimeInterval deadline)
{
    return (SleepConditionVariableSRW(pCondVar, pLock, INFINITE, 0) != 0) ? EOK : EINTR;
}
