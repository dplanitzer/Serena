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

// Signals the given condition variable. Atomically and simultaneously unlocks
// 'pLock' if it is not null. As many VPs are woken up as there are cores in
// the machine.
void ConditionVariable_WakeAndUnlock(ConditionVariable* _Nonnull pCondVar, Lock* _Nullable pLock, bool broadcast)
{
    const int sps = VirtualProcessorScheduler_DisablePreemption();
    const int scs = VirtualProcessorScheduler_DisableCooperation();
    
    if (pLock) {
        Lock_Unlock(pLock);
    }
    
    VirtualProcessorScheduler_RestoreCooperation(scs);
    VirtualProcessorScheduler_WakeUpSome(gVirtualProcessorScheduler,
                                         &pCondVar->wait_queue,
                                         (broadcast) ? INT_MAX : 1,
                                         WAKEUP_REASON_FINISHED,
                                         true);
    VirtualProcessorScheduler_RestorePreemption(sps);
}

// Unlocks 'pLock' and blocks the caller until the condition variable is signaled.
// It then locks 'pLock' before it returns to the caller.
errno_t ConditionVariable_Wait(ConditionVariable* _Nonnull pCondVar, Lock* _Nonnull pLock, struct timespec deadline)
{
    const int sps = VirtualProcessorScheduler_DisablePreemption();
    const int scs = VirtualProcessorScheduler_DisableCooperation();
    
    Lock_Unlock(pLock);
    VirtualProcessorScheduler_RestoreCooperation(scs);
    const int err = VirtualProcessorScheduler_WaitOn(gVirtualProcessorScheduler,
                                                     &pCondVar->wait_queue,
                                                     &deadline,
                                                     true);
    Lock_Lock(pLock);

    VirtualProcessorScheduler_RestorePreemption(sps);
    
    return err;
}
