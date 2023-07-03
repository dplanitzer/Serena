//
//  Lock.c
//  Apollo
//
//  Created by Dietmar Planitzer on 3/27/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "Lock.h"
#include "Heap.h"
#include "VirtualProcessor.h"
#include "VirtualProcessorScheduler.h"


// Initializes a new lock.
void Lock_Init(Lock*_Nonnull pLock)
{
    pLock->value = 0;
    List_Init(&pLock->wait_queue);
    pLock->owner_vpid = 0;
}

// Deinitializes a lock.
void Lock_Deinit(Lock* _Nonnull pLock)
{
    pLock->value = 0;
    List_Deinit(&pLock->wait_queue);
    pLock->owner_vpid = 0;
}

// Allocates a new lock.
Lock* _Nullable Lock_Create(void)
{
    Lock* pLock = (Lock*)kalloc(sizeof(Lock), 0);
    
    if (pLock) {
        Lock_Init(pLock);
    }
    
    return pLock;
}

// Deallocates a lock.
void Lock_Destroy(Lock* _Nullable pLock)
{
    if (pLock) {
        Lock_Deinit(pLock);
        kfree((Byte*)pLock);
    }
}

// Invoked when the Lock_Lock() or Lock_Unlock() functions detected a lock
// ownership violation. Eg VP A has the lock locked but VP B attempts to unlock
// it. Note that this function may not return.
void Lock_OnOwnershipViolation(Lock* _Nonnull pLock)
{
    abort();
}

// Invoked by Lock_Lock() if the lock is currently being held by some other VP.
void Lock_OnWait(Lock* _Nonnull pLock, VirtualProcessorScheduler* _Nonnull pScheduler)
{
    // XXX uninterruptable for now
    assert(VirtualProcessorScheduler_WaitOn(pScheduler, &pLock->wait_queue, kTimeInterval_Infinity, false) == EOK);
}

// Invoked by Lock_Unlock(). Expects to be called with preemption disabled.
void Lock_WakeUp(Lock* _Nullable pLock, VirtualProcessorScheduler* _Nonnull pScheduler)
{
    VirtualProcessorScheduler_WakeUpAll(pScheduler,
                                        &pLock->wait_queue,
                                        true);
}
