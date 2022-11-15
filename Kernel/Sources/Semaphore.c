//
//  Semaphore.c
//  Apollo
//
//  Created by Dietmar Planitzer on 2/10/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "Semaphore.h"
#include "Heap.h"
#include "InterruptController.h"
#include "VirtualProcessorScheduler.h"


extern void BinarySemaphore_InitValue(BinarySemaphore* _Nonnull pSemaphore, UInt val);



// Initializes a new binary semaphore. If 'isAvailable' is true then the semaphore
// starts out in released state.
void BinarySemaphore_Init(BinarySemaphore* _Nonnull pSema, Bool isAvailable)
{
    BinarySemaphore_InitValue(pSema, (isAvailable) ? 0 : 1);
    List_Init(&pSema->wait_queue);
}

// Deinitializes a binary semaphore.
void BinarySemaphore_Deinit(BinarySemaphore* _Nonnull pSema)
{
    pSema->value = 0;
    List_Deinit(&pSema->wait_queue);
}

// Allocates a new binary semaphore.
BinarySemaphore* _Nullable BinarySemaphore_Create(Bool isAvailable)
{
    BinarySemaphore* pSema = (BinarySemaphore*)kalloc(sizeof(BinarySemaphore), 0);
    
    if (pSema) {
        BinarySemaphore_Init(pSema, isAvailable);
    }
    
    return pSema;
}

// Deallocates a binary semaphore.
void BinarySemaphore_Destroy(BinarySemaphore* _Nullable pSema)
{
    if (pSema) {
        BinarySemaphore_Deinit(pSema);
        kfree((Byte*)pSema);
    }
}

// Invoked by BinarySemaphore_Acquire() if the semaphore is currently held by some other VP.
ErrorCode BinarySemaphore_OnWaitForPermit(BinarySemaphore* _Nonnull pSema, VirtualProcessorScheduler* _Nonnull pScheduler, TimeInterval deadline)
{
    // XXX uninterruptable for now because Lock_Lock() uses this
    return VirtualProcessorScheduler_WaitOn(pScheduler, &pSema->wait_queue, deadline, false);
}

// Invoked by BinarySemaphore_Release(). Expects to be called with preemption disabled.
void BinarySemaphore_WakeUp(BinarySemaphore* _Nullable pSema, VirtualProcessorScheduler* _Nonnull pScheduler)
{
    // Do not allow an immediate context switch if we are running in the interrupt
    // context. We'll do the context switch on the way out from the IRQ handler.
    VirtualProcessorScheduler_WakeUpAll(pScheduler,
                                        &pSema->wait_queue,
                                        true);
}


////////////////////////////////////////////////////////////////////////////////

// Creates a new semaphore with the given starting value.
Semaphore* _Nullable Semaphore_Create(Int value)
{
    Semaphore* pSemaphore = (Semaphore*)kalloc(sizeof(Semaphore), 0);
    
    if (pSemaphore) {
        Semaphore_Init(pSemaphore, value);
    }
    return pSemaphore;
}

void Semaphore_Destroy(Semaphore* _Nullable pSemaphore)
{
    if (pSemaphore) {
        Semaphore_Deinit(pSemaphore);
        kfree((Byte*)pSemaphore);
    }
}

// Initializes a new semaphore with 'value' permits
void Semaphore_Init(Semaphore* _Nonnull pSemaphore, Int value)
{
    pSemaphore->value = value;
    List_Init(&pSemaphore->wait_queue);
}

void Semaphore_Deinit(Semaphore* _Nonnull pSemaphore)
{
    List_Deinit(&pSemaphore->wait_queue);
}

void Semaphore_Release(Semaphore* _Nonnull pSemaphore)
{
    Semaphore_ReleaseMultiple(pSemaphore, 1);
}

ErrorCode Semaphore_Acquire(Semaphore* _Nonnull pSemaphore, TimeInterval deadline)
{
    return Semaphore_AcquireMultiple(pSemaphore, 1, deadline);
}

Bool Semaphore_TryAcquire(Semaphore* _Nonnull pSemaphore)
{
    return Semaphore_TryAcquireMultiple(pSemaphore, 1);
}

// Invoked by Semaphore_Acquire() if the semaphore doesn't have the expected number
// of permits.
// Expects to be called with preemption disabled.
ErrorCode Semaphore_OnWaitForPermits(Semaphore* _Nonnull pSemaphore, VirtualProcessorScheduler* _Nonnull pScheduler, TimeInterval deadline)
{
    return VirtualProcessorScheduler_WaitOn(pScheduler, &pSemaphore->wait_queue, deadline, true);
}

// Invoked by Semaphore_Release(). Expects to be called with preemption disabled.
void Semaphore_WakeUp(Semaphore* _Nullable pSemaphore, VirtualProcessorScheduler* _Nonnull pScheduler)
{
    // Do not allow an immediate context switch if we are running in the interrupt
    // context. We'll do the context switch on the way out from the IRQ handler.
    VirtualProcessorScheduler_WakeUpAll(pScheduler,
                                        &pSemaphore->wait_queue,
                                        true);
}
