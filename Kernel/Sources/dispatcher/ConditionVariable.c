//
//  ConditionVariable.c
//  kernel
//
//  Created by Dietmar Planitzer on 4/7/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "ConditionVariable.h"
#include "VirtualProcessorScheduler.h"
#include <kern/limits.h>

// Initializes a new condition variable.
void ConditionVariable_Init(ConditionVariable* _Nonnull pCondVar)
{
    List_Init(&pCondVar->wait_queue);
}

// Deinitializes the condition variables. All virtual processors that are still
// waiting on the conditional variable are woken up with an EINTR error.
void ConditionVariable_Deinit(ConditionVariable* _Nonnull pCondVar)
{
        if (!List_IsEmpty(&pCondVar->wait_queue)) {
        // Wake up all the guys that are still waiting on us and tell them that the
        // wait has been interrupted.
        const int sps = VirtualProcessorScheduler_DisablePreemption();
        VirtualProcessorScheduler_WakeUpSome(gVirtualProcessorScheduler,
                                             &pCondVar->wait_queue,
                                             INT_MAX,
                                             WAKEUP_REASON_INTERRUPTED,
                                             true);
        VirtualProcessorScheduler_RestorePreemption(sps);
    }

    List_Deinit(&pCondVar->wait_queue);
}

// Signals the given condition variable.
void _ConditionVariable_Wakeup(ConditionVariable* _Nonnull pCondVar, bool broadcast)
{
    const int sps = VirtualProcessorScheduler_DisablePreemption();
    
    VirtualProcessorScheduler_WakeUpSome(gVirtualProcessorScheduler,
                                         &pCondVar->wait_queue,
                                         (broadcast) ? INT_MAX : 1,
                                         WAKEUP_REASON_FINISHED,
                                         true);
    VirtualProcessorScheduler_RestorePreemption(sps);
}

// Unlocks 'pLock' and blocks the caller until the condition variable is signaled.
// It then locks 'pLock' before it returns to the caller.
errno_t ConditionVariable_Wait(ConditionVariable* _Nonnull pCondVar, Lock* _Nonnull pLock)
{
    // Note that we disable preemption while unlocking and entering the wait.
    // The reason is that we want to ensure that noone else can grab the lock,
    // do a signal and then unlock between us unlocking and trying to enter the
    // wait. If we would allow this, then we would miss a wakeup. An alternative
    // strategy to this one here would be to use a stateful wait (aka signalling
    // wait).
    const int sps = VirtualProcessorScheduler_DisablePreemption();
    
    Lock_Unlock(pLock);
    const int err = VirtualProcessorScheduler_WaitOn(gVirtualProcessorScheduler,
                                                     &pCondVar->wait_queue,
                                                     WAIT_INTERRUPTABLE,
                                                     NULL,
                                                     NULL);
    Lock_Lock(pLock);

    VirtualProcessorScheduler_RestorePreemption(sps);
    
    return err;
}

// Unlocks 'pLock' and blocks the caller until the condition variable is signaled.
// It then locks 'pLock' before it returns to the caller.
errno_t ConditionVariable_TimedWait(ConditionVariable* _Nonnull pCondVar, Lock* _Nonnull pLock, const struct timespec* _Nonnull deadline)
{
    const int sps = VirtualProcessorScheduler_DisablePreemption();
    
    Lock_Unlock(pLock);
    const int err = VirtualProcessorScheduler_WaitOn(gVirtualProcessorScheduler,
                                                     &pCondVar->wait_queue,
                                                     WAIT_INTERRUPTABLE | WAIT_ABSTIME,
                                                     deadline,
                                                     NULL);
    Lock_Lock(pLock);

    VirtualProcessorScheduler_RestorePreemption(sps);
    
    return err;
}
