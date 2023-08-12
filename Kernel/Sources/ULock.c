//
//  ULock.c
//  Apollo
//
//  Created by Dietmar Planitzer on 8/12/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "ULock.h"
#include "kalloc.h"
#include "VirtualProcessorScheduler.h"


// Initializes a new lock.
void ULock_Init(ULock*_Nonnull pLock)
{
    pLock->value = 0;
    List_Init(&pLock->wait_queue);
    pLock->owner_vpid = 0;
}

// Deinitializes a lock. The lock is automatically unlocked if the calling code
// is holding the lock.
ErrorCode ULock_Deinit(ULock* _Nonnull pLock)
{
    // Unlock the lock if it is currently held by the virtual processor on which
    // we are executing. Abort if a virtual processor which isn't holding the
    // lock tries to destroy it.
    const Int ownerId = ULock_GetOwnerVpid(pLock);
    if (ownerId == VirtualProcessor_GetCurrentVpid()) {
        ULock_Unlock(pLock);
    } else if (ownerId > 0) {
        return EPERM;
    }

    pLock->value = 0;
    List_Deinit(&pLock->wait_queue);
    pLock->owner_vpid = 0;

    return EOK;
}

// Allocates a new lock.
ErrorCode _Nullable ULock_Create(ULock* _Nullable * _Nonnull pOutLock)
{
    decl_try_err();
    ULock* pLock = NULL;
    
    try(kalloc(sizeof(ULock), (Byte**) &pLock));
    ULock_Init(pLock);
    
    *pOutLock = pLock;
    return EOK;

catch:
    *pOutLock = NULL;
    return err;
}

// Deallocates a lock. The lock is automatically unlocked if the calling code
// is holding the lock.
ErrorCode ULock_Destroy(ULock* _Nullable pLock)
{
    decl_try_err();

    if (pLock) {
        try(ULock_Deinit(pLock));
        kfree((Byte*)pLock);
    }

catch:
    return err;
}

// Invoked by ULock_Lock() if the lock is currently being held by some other VP.
ErrorCode ULock_OnWait(ULock* _Nonnull pLock, UInt options)
{
    const Bool isInterruptable = (options & ULOCK_OPTION_INTERRUPTABLE) != 0;

    return VirtualProcessorScheduler_WaitOn(gVirtualProcessorScheduler,
                                            &pLock->wait_queue,
                                            kTimeInterval_Infinity,
                                            isInterruptable);
}

// Invoked by ULock_Unlock(). Expects to be called with preemption disabled.
void ULock_WakeUp(ULock* _Nullable pLock)
{
    VirtualProcessorScheduler_WakeUpAll(gVirtualProcessorScheduler,
                                        &pLock->wait_queue,
                                        true);
}
