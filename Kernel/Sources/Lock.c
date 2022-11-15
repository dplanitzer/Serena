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


// Initializes a new lock.
void Lock_Init(Lock*_Nonnull pLock)
{
    BinarySemaphore_Init(&pLock->sema, true);
    pLock->owner_vpid = 0;
}

// Deinitializes a lock.
void Lock_Deinit(Lock* _Nonnull pLock)
{
    BinarySemaphore_Deinit(&pLock->sema);
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

// Blocks the caller until the lock can be taken.
void Lock_Lock(Lock* _Nonnull pLock)
{
    BinarySemaphore_Acquire(&pLock->sema, kTimeInterval_Infinity);

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

    BinarySemaphore_Release(&pLock->sema);
}
