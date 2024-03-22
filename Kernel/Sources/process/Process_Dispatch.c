//
//  Process_Dispatch.c
//  kernel
//
//  Created by Dietmar Planitzer on 3/21/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "ProcessPriv.h"
#include "ULock.h"


// Creates a new ULock and binds it to the process.
errno_t Process_CreateULock(ProcessRef _Nonnull pProc, int* _Nullable pOutLock)
{
    decl_try_err();
    ULockRef pLock = NULL;

    Lock_Lock(&pProc->lock);

    *pOutLock = -1;
    try(ULock_Create(&pLock));
    try(Process_RegisterPrivateResource_Locked(pProc, (ObjectRef) pLock, pOutLock));

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
