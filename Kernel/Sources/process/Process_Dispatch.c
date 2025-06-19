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
errno_t Process_CreateUConditionVariable(ProcessRef _Nonnull self, int* _Nullable pOutOd)
{
    decl_try_err();
    UConditionVariableRef pCV = NULL;

    try(UConditionVariable_Create(&pCV));
    try(UResourceTable_AdoptResource(&self->uResourcesTable, (UResourceRef) pCV, pOutOd));
    pCV = NULL;
    return EOK;

catch:
    UResource_Dispose(pCV);
    *pOutOd = -1;
    return err;
}

// Wakes the given condition variable and unlock the associated lock if
// 'dLock' is not -1. This does a signal or broadcast.
errno_t Process_WakeUConditionVariable(ProcessRef _Nonnull self, int odCV, int odLock, bool bBroadcast)
{
    decl_try_err();
    UConditionVariableRef pCV = NULL;
    ULockRef pLock = NULL;

    if ((err = UResourceTable_AcquireTwoResourcesAs(&self->uResourcesTable, odCV, UConditionVariable, &pCV, odLock, ULock, &pLock)) == EOK) {
        UConditionVariable_WakeAndUnlock(pCV, pLock, bBroadcast);
        UResourceTable_RelinquishTwoResources(&self->uResourcesTable, pCV, pLock);
    }
    return err;
}

// Blocks the caller until the condition variable has received a signal or the
// wait has timed out. Automatically and atomically acquires the associated
// lock on wakeup. An ETIMEOUT error is returned if the condition variable is
// not signaled before 'deadline'.
errno_t Process_WaitUConditionVariable(ProcessRef _Nonnull self, int odCV, int odLock, struct timespec deadline)
{
    decl_try_err();
    UConditionVariableRef pCV = NULL;
    ULockRef pLock = NULL;

    if ((err = UResourceTable_AcquireTwoResourcesAs(&self->uResourcesTable, odCV, UConditionVariable, &pCV, odLock, ULock, &pLock)) == EOK) {
        err = UConditionVariable_Wait(pCV, pLock, deadline);
        UResourceTable_RelinquishTwoResources(&self->uResourcesTable, pCV, pLock);
    }
    return err;
}


// Creates a new ULock and binds it to the process.
errno_t Process_CreateULock(ProcessRef _Nonnull self, int* _Nullable pOutOd)
{
    decl_try_err();
    ULockRef pLock = NULL;

    try(ULock_Create(&pLock));
    try(UResourceTable_AdoptResource(&self->uResourcesTable, (UResourceRef) pLock, pOutOd));
    pLock = NULL;
    return EOK;

catch:
    UResource_Dispose(pLock);
    *pOutOd = -1;
    return err;
}

// Tries taking the given lock. Returns EOK on success and EBUSY if someone else
// is already holding the lock.
errno_t Process_TryULock(ProcessRef _Nonnull self, int od)
{
    decl_try_err();
    ULockRef pLock;

    if ((err = UResourceTable_BeginDirectResourceAccessAs(&self->uResourcesTable, od, ULock, &pLock)) == EOK) {
        err = (ULock_TryLock(pLock)) ? EOK : EBUSY;
        UResourceTable_EndDirectResourceAccess(&self->uResourcesTable);
    }
    return err;
}

// Locks the given user lock. The caller will remain blocked until the lock can
// be successfully acquired or the wait is interrupted for some reason.
errno_t Process_LockULock(ProcessRef _Nonnull self, int od)
{
    decl_try_err();
    ULockRef pLock;

    if ((err = UResourceTable_AcquireResourceAs(&self->uResourcesTable, od, ULock, &pLock)) == EOK) {
        err = ULock_Lock(pLock);
        UResourceTable_RelinquishResource(&self->uResourcesTable, pLock);
    }
    return err;
}

// Unlocks the given user lock. Returns EOK on success and EPERM if the lock is
// currently being held by some other virtual processor.
errno_t Process_UnlockULock(ProcessRef _Nonnull self, int od)
{
    decl_try_err();
    ULockRef pLock;

    if ((err = UResourceTable_BeginDirectResourceAccessAs(&self->uResourcesTable, od, ULock, &pLock)) == EOK) {
        err = ULock_Unlock(pLock);
        UResourceTable_EndDirectResourceAccess(&self->uResourcesTable);
    }
    return err;
}


// Creates a new USemaphore and binds it to the process.
errno_t Process_CreateUSemaphore(ProcessRef _Nonnull self, int npermits, int* _Nullable pOutOd)
{
    decl_try_err();
    USemaphoreRef pSema = NULL;

    try(USemaphore_Create(npermits, &pSema));
    try(UResourceTable_AdoptResource(&self->uResourcesTable, (UResourceRef) pSema, pOutOd));
    pSema = NULL;
    return EOK;

catch:
    UResource_Dispose(pSema);
    *pOutOd = -1;
    return err;
}

// Releases 'npermits' permits to the semaphore.
errno_t Process_RelinquishUSemaphore(ProcessRef _Nonnull self, int od, int npermits)
{
    decl_try_err();
    USemaphoreRef pSema;

    if ((err = UResourceTable_BeginDirectResourceAccessAs(&self->uResourcesTable, od, USemaphore, &pSema)) == EOK) {
        USemaphore_Relinquish(pSema, npermits);
        UResourceTable_EndDirectResourceAccess(&self->uResourcesTable);
    }
    return err;
}

// Blocks the caller until 'npermits' can be successfully acquired from the given
// semaphore. Returns EOK on success and ETIMEOUT if the permits could not be
// acquired before 'deadline'.
errno_t Process_AcquireUSemaphore(ProcessRef _Nonnull self, int od, int npermits, const struct timespec* _Nonnull deadline)
{
    decl_try_err();
    USemaphoreRef pSema;

    if ((err = UResourceTable_AcquireResourceAs(&self->uResourcesTable, od, USemaphore, &pSema)) == EOK) {
        err = USemaphore_Acquire(pSema, npermits, deadline);
        UResourceTable_RelinquishResource(&self->uResourcesTable, pSema);
    }
    return err;
}

// Tries to acquire 'npermits' from the given semaphore. Returns true on success
// and false otherwise. This function does not block the caller.
errno_t Process_TryAcquireUSemaphore(ProcessRef _Nonnull self, int npermits, int od)
{
    decl_try_err();
    USemaphoreRef pSema;

    if ((err = UResourceTable_BeginDirectResourceAccessAs(&self->uResourcesTable, od, USemaphore, &pSema)) == EOK) {
        err = USemaphore_TryAcquire(pSema, npermits) ? EOK : EBUSY;
        UResourceTable_EndDirectResourceAccess(&self->uResourcesTable);
    }
    return err;
}
