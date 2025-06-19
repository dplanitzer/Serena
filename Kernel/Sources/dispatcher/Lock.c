//
//  Lock.c
//  kernel
//
//  Created by Dietmar Planitzer on 3/27/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "Lock.h"
#include "VirtualProcessorScheduler.h"
#include <kern/limits.h>
#include <kern/timespec.h>


void Lock_Init(Lock* _Nonnull self)
{
    Lock_InitWithOptions(self, kLockOption_FatalOwnershipViolations);
}

void Lock_InitWithOptions(Lock*_Nonnull self, uint32_t options)
{
    self->value = 0;
    List_Init(&self->wait_queue);
    self->owner_vpid = 0;
    self->options = options;
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
        if ((self->options & kLockOption_FatalOwnershipViolations) != 0) {
            fatalError(__func__, __LINE__, EPERM);
        }
        else {
            return EPERM;
        }
    }

    self->value = 0;
    List_Deinit(&self->wait_queue);
    self->owner_vpid = 0;

    return EOK;
}

// Unlocks the lock.
errno_t Lock_Unlock(Lock* _Nonnull self)
{
    extern errno_t _Lock_Unlock(Lock* _Nonnull self);

    const errno_t err = _Lock_Unlock(self);

    if (err == EOK || (self->options & kLockOption_FatalOwnershipViolations) == 0) {
        return err;
    }
    else {
        fatalError(__func__, __LINE__, err);
    }
}

// Invoked by Lock_Lock() if the lock is currently being held by some other VP.
errno_t Lock_OnWait(Lock* _Nonnull self)
{
    const bool isInterruptable = (self->options & kLockOption_InterruptibleLock) != 0 ? true : false;
    const errno_t err = VirtualProcessorScheduler_WaitOn(gVirtualProcessorScheduler,
                                            &self->wait_queue,
                                            &TIMESPEC_INF,
                                            isInterruptable);

    if (err == EOK || isInterruptable) {
        return err;
    }
    else {
        fatalError(__func__, __LINE__, err);
    }
}

// Invoked by Lock_Unlock(). Expects to be called with preemption disabled.
void Lock_WakeUp(Lock* _Nullable self)
{
    VirtualProcessorScheduler_WakeUpAll(gVirtualProcessorScheduler,
                                        &self->wait_queue,
                                        true);
}
