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
    ConditionVariable* pCondVar = (ConditionVariable*)kalloc(sizeof(ConditionVariable), 0);
    
    if (pCondVar) {
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

void ConditionVariable_Deinit(ConditionVariable* _Nonnull pCondVar)
{
    List_Deinit(&pCondVar->wait_queue);
}

// Signals the given condition variable. Atomically unlock 'pLock' at the same
// time if 'pLock' is not null. As many VPs are woken up as there are cores in
// the machine.
void ConditionVariable_Signal(ConditionVariable* _Nonnull pCondVar, Lock* _Nullable pLock)
{
    const Int sps = VirtualProcessorScheduler_DisablePreemption();
    const Int scs = VirtualProcessorScheduler_DisableCooperation();
    
    if (pLock) {
        Lock_Unlock(pLock);
    }
    VirtualProcessorScheduler_RestoreCooperation(scs);
    VirtualProcessorScheduler_WakeUpSome(VirtualProcessorScheduler_GetShared(), &pCondVar->wait_queue, 1, WAKEUP_REASON_FINISHED, true);
    VirtualProcessorScheduler_RestorePreemption(sps);
}

// Wakes up all VPs that are waiting on 'pCondVar'. Atomically unlocks 'pLock'
// at the same time if 'pLock' is not null.
void ConditionVariable_Broadcast(ConditionVariable* _Nonnull pCondVar, Lock* _Nullable pLock)
{
    const Int sps = VirtualProcessorScheduler_DisablePreemption();
    const Int scs = VirtualProcessorScheduler_DisableCooperation();

    if (pLock) {
        Lock_Unlock(pLock);
    }
    VirtualProcessorScheduler_RestoreCooperation(scs);
    VirtualProcessorScheduler_WakeUpAll(VirtualProcessorScheduler_GetShared(), &pCondVar->wait_queue, true);
    VirtualProcessorScheduler_RestorePreemption(sps);
}

// Unlock 'pLock' and blocks teh caller until the condition variable is signaled.
// It then locks 'pLock' before it returns to the caller.
ErrorCode ConditionVariable_Wait(ConditionVariable* _Nonnull pCondVar, Lock* _Nonnull pLock, TimeInterval deadline)
{
    const Int sps = VirtualProcessorScheduler_DisablePreemption();
    const Int scs = VirtualProcessorScheduler_DisableCooperation();
    
    Lock_Unlock(pLock);
    VirtualProcessorScheduler_RestoreCooperation(scs);
    const Int err = VirtualProcessorScheduler_WaitOn(VirtualProcessorScheduler_GetShared(), &pCondVar->wait_queue, deadline, true);
    Lock_Lock(pLock);

    VirtualProcessorScheduler_RestorePreemption(sps);
    
    return err;
}
