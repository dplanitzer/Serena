//
//  ConditionVariable.c
//  Apollo
//
//  Created by Dietmar Planitzer on 4/7/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "ConditionVariable.h"
#include "Heap.h"
#include "VirtualProcessorScheduler.h"


// Creates a new condition variable.
ConditionVariable* _Nullable ConditionVariable_Create(void)
{
    ConditionVariable* pCondVar;
    
    if (kalloc(sizeof(ConditionVariable), (Byte**) &pCondVar) == EOK) {
        ConditionVariable_Init(pCondVar);
        pCondVar->name[0] = '\0';
    }
    return pCondVar;
}

void ConditionVariable_Destroy(ConditionVariable* _Nullable pCondVar)
{
    if (pCondVar) {
        ConditionVariable_Deinit(pCondVar);
        kfree((Byte*)pCondVar);
    }
}

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
        const Int sps = VirtualProcessorScheduler_DisablePreemption();
        VirtualProcessorScheduler_WakeUpSome(VirtualProcessorScheduler_GetShared(),
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
void ConditionVariable_SignalAndUnlock(ConditionVariable* _Nonnull pCondVar, Lock* _Nullable pLock)
{
    const Int sps = VirtualProcessorScheduler_DisablePreemption();
    const Int scs = VirtualProcessorScheduler_DisableCooperation();
    
    if (pLock) {
        Lock_Unlock(pLock);
    }
    VirtualProcessorScheduler_RestoreCooperation(scs);
    VirtualProcessorScheduler_WakeUpSome(VirtualProcessorScheduler_GetShared(),
                                         &pCondVar->wait_queue,
                                         1,
                                         WAKEUP_REASON_FINISHED,
                                         true);
    VirtualProcessorScheduler_RestorePreemption(sps);
}

// Wakes up all VPs that are waiting on 'pCondVar'. Atomically and simultaneously
// unlocks 'pLock' if it is not null.
void ConditionVariable_BroadcastAndUnlock(ConditionVariable* _Nonnull pCondVar, Lock* _Nullable pLock)
{
    const Int sps = VirtualProcessorScheduler_DisablePreemption();
    const Int scs = VirtualProcessorScheduler_DisableCooperation();

    if (pLock) {
        Lock_Unlock(pLock);
    }
    VirtualProcessorScheduler_RestoreCooperation(scs);
    VirtualProcessorScheduler_WakeUpAll(VirtualProcessorScheduler_GetShared(),
                                        &pCondVar->wait_queue,
                                        true);
    VirtualProcessorScheduler_RestorePreemption(sps);
}

// Unlocks 'pLock' and blocks the caller until the condition variable is signaled.
// It then locks 'pLock' before it returns to the caller.
ErrorCode ConditionVariable_Wait(ConditionVariable* _Nonnull pCondVar, Lock* _Nonnull pLock, TimeInterval deadline)
{
    const Int sps = VirtualProcessorScheduler_DisablePreemption();
    const Int scs = VirtualProcessorScheduler_DisableCooperation();
    
    Lock_Unlock(pLock);
    VirtualProcessorScheduler_RestoreCooperation(scs);
    const Int err = VirtualProcessorScheduler_WaitOn(VirtualProcessorScheduler_GetShared(),
                                                     &pCondVar->wait_queue,
                                                     deadline);
    Lock_Lock(pLock);

    VirtualProcessorScheduler_RestorePreemption(sps);
    
    return err;
}
