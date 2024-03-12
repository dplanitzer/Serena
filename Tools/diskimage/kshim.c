//
//  kshim.c
//  diskimage
//
//  Created by Dietmar Planitzer on 3/10/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "diskimage.h"
#include <stdlib.h>

TimeInterval MonotonicClock_GetCurrentTime(void)
{
    TimeInterval ti;

    timespec_get(&ti, TIME_UTC);
    return ti;
}


errno_t kalloc(size_t nbytes, void** pOutPtr)
{
    *pOutPtr = malloc(nbytes);
    return (*pOutPtr) ? EOK : ENOMEM;
}

void kfree(void* ptr)
{
    free(ptr);
}


errno_t Filesystem_Create(ClassRef _Nonnull pClass, FilesystemRef _Nullable * _Nonnull pOutFileSys)
{
    *pOutFileSys = calloc(1, pClass->instanceSize);
    return (*pOutFileSys) ? EOK : ENOMEM;
}


void Lock_Init(Lock* _Nonnull pLock)
{
    InitializeSRWLock(pLock);
}   

void Lock_Deinit(Lock* _Nonnull pLock)
{
}

void Lock_Lock(Lock* _Nonnull pLock)
{
    AcquireSRWLockExclusive(pLock);
}

void Lock_Unlock(Lock* _Nonnull pLock)
{
    ReleaseSRWLockExclusive(pLock);
}


void ConditionVariable_Init(ConditionVariable* _Nonnull pCondVar)
{
    InitializeConditionVariable(pCondVar);
}

void ConditionVariable_Deinit(ConditionVariable* _Nonnull pCondVar)
{
}

void ConditionVariable_BroadcastAndUnlock(ConditionVariable* _Nonnull pCondVar, Lock* _Nullable pLock)
{
    WakeAllConditionVariable(pCondVar);
    ReleaseSRWLockExclusive(pLock);
}

errno_t ConditionVariable_Wait(ConditionVariable* _Nonnull pCondVar, Lock* _Nonnull pLock, TimeInterval deadline)
{
    return (SleepConditionVariableSRW(pCondVar, pLock, INFINITE, 0) != 0) ? EOK : EINTR;
}
