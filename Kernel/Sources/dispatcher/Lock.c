//
//  Lock.c
//  kernel
//
//  Created by Dietmar Planitzer on 3/27/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "Lock.h"

extern errno_t _Lock_Unlock(Lock* _Nonnull self);


void Lock_Init(Lock* _Nonnull self)
{
    self->value = 0;
    WaitQueue_Init(&self->wq);
    self->owner_vpid = 0;
}

void Lock_Deinit(Lock* _Nonnull self)
{
    assert(Lock_GetOwnerVpid(self) == 0);
    assert(WaitQueue_Deinit(&self->wq) == EOK);
}

// Unlocks the lock.
errno_t Lock_Unlock(Lock* _Nonnull self)
{
    const errno_t err = _Lock_Unlock(self);
    if (err == EOK) {
        return err;
    }

    fatalError(__func__, __LINE__, err);
}

// Invoked by Lock_Lock() if the lock is currently being held by some other VP.
// @Entry Condition: preemption disabled
errno_t Lock_OnWait(Lock* _Nonnull self)
{
    const errno_t err = WaitQueue_Wait(&self->wq, 0, NULL, NULL);
    if (err == EOK) {
        return err;
    }

    fatalError(__func__, __LINE__, err);
}

// Invoked by Lock_Unlock(). Expects to be called with preemption disabled.
// @Entry Condition: preemption disabled
void Lock_WakeUp(Lock* _Nullable self)
{
    WaitQueue_WakeUpAll(&self->wq, true);
}
