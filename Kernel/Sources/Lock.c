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


extern void _Lock_Lock(Lock* _Nonnull pLock);
extern void _Lock_Unlock(Lock* _Nonnull pLock);


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

// Blocks the caller until the lock can be taken successfully.
void Lock_Lock(Lock* _Nonnull pLock)
{
    _Lock_Lock(pLock);

    if (pLock->owner_vpid == 0) {
        pLock->owner_vpid = VirtualProcessor_GetCurrent()->vpid;
    } else {
        abort();
    }
}

// Unlocks the lock.
void Lock_Unlock(Lock* _Nonnull pLock)
{
    if (pLock->owner_vpid != 0) {
        pLock->owner_vpid = 0;
    } else {
        abort();
    }

    _Lock_Unlock(pLock);
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
