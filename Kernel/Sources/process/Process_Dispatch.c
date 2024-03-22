//
//  Process_Dispatch.c
//  kernel
//
//  Created by Dietmar Planitzer on 3/21/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "ProcessPriv.h"
#include "UConditionVariable.h"
#include "ULock.h"
#include "USemaphore.h"


// Creates a new UConditionVariable and binds it to the process.
errno_t Process_CreateUConditionVariable(ProcessRef _Nonnull pProc, int* _Nullable pOutOd)
{
    decl_try_err();
    UConditionVariableRef pCV = NULL;

    Lock_Lock(&pProc->lock);

    *pOutOd = -1;
    try(UConditionVariable_Create(&pCV));
    try(Process_RegisterPrivateResource_Locked(pProc, (ObjectRef) pCV, pOutOd));

catch:
    Object_Release(pCV);
    Lock_Unlock(&pProc->lock);
    return err;
}

// Wakes the given condition variable and unlock the associated lock if
// 'dLock' is not -1. This does a signal or broadcast.
errno_t Process_WakeUConditionVariable(ProcessRef _Nonnull pProc, int od, int dLock, bool bBroadcast)
{
    decl_try_err();
    UConditionVariableRef pCV = NULL;
    ULockRef pLock = NULL;

    if ((err = Process_CopyPrivateResourceForDescriptor(pProc, od, (ObjectRef*)&pCV)) == EOK) {
        if (dLock != -1) {
            err = Process_CopyPrivateResourceForDescriptor(pProc, dLock, (ObjectRef*)&pLock);
        }
        if (err == EOK) {
            if (bBroadcast) {
                UConditionVariable_BroadcastAndUnlock(pCV, pLock);
            }
            else {
                UConditionVariable_SignalAndUnlock(pCV, pLock);
            }
        }
        Object_Release(pCV);
        Object_Release(pLock);
    }
    return err;
}

// Blocks the caller until the condition variable has received a signal or the
// wait has timed out. Automatically and atomically acquires the associated
// lock on wakeup. An ETIMEOUT error is returned if teh condition variable is
// not signaled before 'deadline'.
errno_t Process_WaitUConditionVariable(ProcessRef _Nonnull pProc, int od, int dLock, TimeInterval deadline)
{
    decl_try_err();
    UConditionVariableRef pCV = NULL;
    ULockRef pLock = NULL;

    if ((err = Process_CopyPrivateResourceForDescriptor(pProc, od, (ObjectRef*)&pCV)) == EOK) {
        if (dLock != -1) {
            err = Process_CopyPrivateResourceForDescriptor(pProc, dLock, (ObjectRef*)&pLock);
        }
        if (err == EOK) {
            err = UConditionVariable_Wait(pCV, pLock, deadline);
        }
        Object_Release(pCV);
        Object_Release(pLock);
    }
    return err;
}


// Creates a new ULock and binds it to the process.
errno_t Process_CreateULock(ProcessRef _Nonnull pProc, int* _Nullable pOutOd)
{
    decl_try_err();
    ULockRef pLock = NULL;

    Lock_Lock(&pProc->lock);

    *pOutOd = -1;
    try(ULock_Create(&pLock));
    try(Process_RegisterPrivateResource_Locked(pProc, (ObjectRef) pLock, pOutOd));

catch:
    Object_Release(pLock);
    Lock_Unlock(&pProc->lock);
    return err;
}

// Tries taking the given lock. Returns EOK on success and EBUSY if someone else
// is already holding the lock.
errno_t Process_TryULock(ProcessRef _Nonnull pProc, int od)
{
    decl_try_err();
    ULockRef pLock;

    if ((err = Process_CopyPrivateResourceForDescriptor(pProc, od, (ObjectRef*) &pLock)) == EOK) {
        err = (ULock_TryLock(pLock)) ? EOK : EBUSY;
        Object_Release(pLock);
    }
    return err;
}

// Locks the given user lock. The caller will remain blocked until the lock can
// be successfully acquired or the wait is interrupted for some reason.
errno_t Process_LockULock(ProcessRef _Nonnull pProc, int od)
{
    decl_try_err();
    ULockRef pLock;

    if ((err = Process_CopyPrivateResourceForDescriptor(pProc, od, (ObjectRef*) &pLock)) == EOK) {
        err = ULock_Lock(pLock);
        Object_Release(pLock);
    }
    return err;
}

// Unlocks the given user lock. Returns EOK on success and EPERM if the lock is
// currently being held by some other virtual processor.
errno_t Process_UnlockULock(ProcessRef _Nonnull pProc, int od)
{
    decl_try_err();
    ULockRef pLock;

    if ((err = Process_CopyPrivateResourceForDescriptor(pProc, od, (ObjectRef*) &pLock)) == EOK) {
        err = ULock_Unlock(pLock);
        Object_Release(pLock);
    }
    return err;
}


// Creates a new USemaphore and binds it to the process.
errno_t Process_CreateUSemaphore(ProcessRef _Nonnull pProc, int npermits, int* _Nullable pOutOd)
{
    decl_try_err();
    USemaphoreRef pSema = NULL;

    Lock_Lock(&pProc->lock);

    *pOutOd = -1;
    try(USemaphore_Create(npermits, &pSema));
    try(Process_RegisterPrivateResource_Locked(pProc, (ObjectRef) pSema, pOutOd));

catch:
    Object_Release(pSema);
    Lock_Unlock(&pProc->lock);
    return err;
}

// Releases 'npermits' permits to the semaphore.
errno_t Process_RelinquishUSemaphore(ProcessRef _Nonnull pProc, int od, int npermits)
{
    decl_try_err();
    USemaphoreRef pSema;

    if ((err = Process_CopyPrivateResourceForDescriptor(pProc, od, (ObjectRef*) &pSema)) == EOK) {
        USemaphore_Relinquish(pSema, npermits);
        Object_Release(pSema);
    }
    return err;
}

// Blocks the caller until 'npermits' can be successfully acquired from the given
// semaphore. Returns EOK on success and ETIMEOUT if the permits could not be
// acquired before 'deadline'.
errno_t Process_AcquireUSemaphore(ProcessRef _Nonnull pProc, int od, int npermits, TimeInterval deadline)
{
    decl_try_err();
    USemaphoreRef pSema;

    if ((err = Process_CopyPrivateResourceForDescriptor(pProc, od, (ObjectRef*) &pSema)) == EOK) {
        err = USemaphore_Acquire(pSema, npermits, deadline);
        Object_Release(pSema);
    }
    return err;
}

// Tries to acquire 'npermits' from the given semaphore. Returns true on success
// and false otherwise. This function does not block the caller.
errno_t Process_TryAcquireUSemaphore(ProcessRef _Nonnull pProc, int npermits, int od)
{
    decl_try_err();
    USemaphoreRef pSema;

    if ((err = Process_CopyPrivateResourceForDescriptor(pProc, od, (ObjectRef*) &pSema)) == EOK) {
        err = USemaphore_TryAcquire(pSema, npermits) ? EOK : EBUSY;
        Object_Release(pSema);
    }
    return err;
}
