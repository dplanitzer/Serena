//
//  ConditionVariable.c
//  kernel
//
//  Created by Dietmar Planitzer on 4/7/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "ConditionVariable.h"
#include "VirtualProcessorScheduler.h"
#include <kern/assert.h>


// Initializes a new condition variable.
void ConditionVariable_Init(ConditionVariable* _Nonnull self)
{
    WaitQueue_Init(&self->wq);
}

// Deinitializes the condition variable.
void ConditionVariable_Deinit(ConditionVariable* _Nonnull self)
{
    assert(WaitQueue_Deinit(&self->wq) == EOK);
}

// Signals the given condition variable.
void _ConditionVariable_Wakeup(ConditionVariable* _Nonnull self, bool broadcast)
{
    const int flags = (broadcast) ? WAKEUP_ALL : WAKEUP_ONE;
    const int sps = preempt_disable();
    
    WaitQueue_Wakeup(&self->wq, flags | WAKEUP_CSW, 0);
    preempt_restore(sps);
}

// Unlocks 'pLock' and blocks the caller until the condition variable is signaled.
// It then locks 'pLock' before it returns to the caller.
errno_t ConditionVariable_Wait(ConditionVariable* _Nonnull self, Lock* _Nonnull pLock)
{
    // Note that we disable preemption while unlocking and entering the wait.
    // The reason is that we want to ensure that noone else can grab the lock,
    // do a signal and then unlock between us unlocking and trying to enter the
    // wait. If we would allow this, then we would miss a wakeup. An alternative
    // strategy to this one here would be to use a stateful wait (aka signalling
    // wait).
    const int sps = preempt_disable();
    
    Lock_Unlock(pLock);
    const int err = WaitQueue_Wait(&self->wq, WAIT_INTERRUPTABLE);
    Lock_Lock(pLock);

    preempt_restore(sps);
    
    return err;
}

// Unlocks 'pLock' and blocks the caller until the condition variable is signaled.
// It then locks 'pLock' before it returns to the caller.
errno_t ConditionVariable_TimedWait(ConditionVariable* _Nonnull self, Lock* _Nonnull pLock, const struct timespec* _Nonnull deadline)
{
    const int sps = preempt_disable();
    
    Lock_Unlock(pLock);
    const int err = WaitQueue_TimedWait(&self->wq,
                                WAIT_INTERRUPTABLE | WAIT_ABSTIME,
                                deadline,
                                NULL);
    Lock_Lock(pLock);

    preempt_restore(sps);
    
    return err;
}
