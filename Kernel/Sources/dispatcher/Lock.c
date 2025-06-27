//
//  Lock.c
//  kernel
//
//  Created by Dietmar Planitzer on 3/27/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "Lock.h"
#include "VirtualProcessorScheduler.h"

extern errno_t _Lock_Unlock(Lock* _Nonnull self);


void Lock_Init(Lock* _Nonnull self)
{
    self->value = 0;
    List_Init(&self->wait_queue);
    self->owner_vpid = 0;
}

errno_t Lock_Deinit(Lock* _Nonnull self)
{
    // Unlock the lock if it is currently held by the virtual processor on which
    // we are executing.
    const int ownerId = Lock_GetOwnerVpid(self);

    if (ownerId == VirtualProcessor_GetCurrentVpid()) {
        Lock_Unlock(self);
    }
    else if (ownerId > 0) {
        fatalError(__func__, __LINE__, EPERM);
    }

    self->value = 0;
    List_Deinit(&self->wait_queue);
    self->owner_vpid = 0;

    return EOK;
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
errno_t Lock_OnWait(Lock* _Nonnull self)
{
    const errno_t err = VirtualProcessorScheduler_WaitOn(gVirtualProcessorScheduler,
                                            &self->wait_queue,
                                            0,
                                            NULL,
                                            NULL);
    if (err == EOK) {
        return err;
    }

    fatalError(__func__, __LINE__, err);
}

// Invoked by Lock_Unlock(). Expects to be called with preemption disabled.
void Lock_WakeUp(Lock* _Nullable self)
{
    VirtualProcessorScheduler_WakeUpAll(gVirtualProcessorScheduler,
                                        &self->wait_queue,
                                        true);
}
