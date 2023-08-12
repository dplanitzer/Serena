//
//  Lock.c
//  Apollo
//
//  Created by Dietmar Planitzer on 3/27/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "Lock.h"


// Deinitializes a lock. The lock is automatically unlocked if the calling code
// is holding the lock.
void Lock_Deinit(Lock* _Nonnull pLock)
{
    try_bang(ULock_Deinit(pLock));
}

// Blocks the caller until the lock can be taken successfully.
void Lock_Lock(Lock* _Nonnull pLock)
{
    // The wait is uninterruptable
    try_bang(ULock_Lock(pLock, 0));
}

// Unlocks the lock.
void Lock_Unlock(Lock* _Nonnull pLock)
{
    try_bang(ULock_Unlock(pLock));
}
