//
//  Lock.h
//  Apollo
//
//  Created by Dietmar Planitzer on 3/27/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef Lock_h
#define Lock_h

#include "Foundation.h"
#include "Semaphore.h"


typedef struct _Lock {
    BinarySemaphore sema;
    Int             owner_vpid;     // ID of the VP that is currently holding the lock
} Lock;


// Initializes a new lock.
extern void Lock_Init(Lock*_Nonnull pLock);

// Deinitializes a lock.
extern void Lock_Deinit(Lock* _Nonnull pLock);

// Allocates a new lock.
extern Lock* _Nullable Lock_Create(void);

// Deallocates a lock.
extern void Lock_Destroy(Lock* _Nullable pLock);

// Blocks the caller until the lock can be taken successfully.
extern void Lock_Lock(Lock* _Nonnull pLock);

// Unlocks the lock.
extern void Lock_Unlock(Lock* _Nonnull pLock);

#endif /* Lock_h */
