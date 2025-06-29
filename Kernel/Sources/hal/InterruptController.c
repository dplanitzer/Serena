//
//  InterruptController.c
//  kernel
//
//  Created by Dietmar Planitzer on 5/5/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "InterruptControllerPriv.h"
#include <log/Log.h>
#include <kern/kalloc.h>


InterruptController     gInterruptControllerStorage;
InterruptControllerRef  gInterruptController = &gInterruptControllerStorage;


errno_t InterruptController_CreateForLocalCPU(void)
{    
    decl_try_err();
    InterruptController* pController = &gInterruptControllerStorage;

    for (int i = 0; i < INTERRUPT_ID_COUNT; i++) {
        try(kalloc(0, (void**) &pController->handlers[i].start));
        pController->handlers[i].count = 0;
    }
    
    pController->nextAvailableId = 1;
    pController->spuriousInterruptCount = 0;
    pController->uninitializedInterruptCount = 0;
    
    Lock_Init(&pController->lock);
    return EOK;

catch:
    return err;
}

static void insertion_sort(InterruptHandler* _Nonnull pHandlers, int n)
{
    for (int i = 1; i < n; i++) {
        const InterruptHandler handler = pHandlers[i];
        int j = i - 1;
        
        while (j >= 0 && pHandlers[j].priority < handler.priority) {
            pHandlers[j + 1] = pHandlers[j];
            j = j - 1;
        }
        pHandlers[j + 1] = handler;
    }
}

// Adds the given interrupt handler to the controller. Returns the ID of the handler.
// 0 is returned if allocation failed.
static errno_t InterruptController_AddInterruptHandler(InterruptControllerRef _Nonnull pController, InterruptID interruptId, InterruptHandler* _Nonnull pHandler, InterruptHandlerID* _Nonnull pOutId)
{
    decl_try_err();
    InterruptHandlerID handlerId = 0;
    bool needsUnlock = false;
    
    assert(pHandler->identity == 0);

    Lock_Lock(&pController->lock);
    needsUnlock = true;

    // Allocate a new handler array
    const int oldCount = pController->handlers[interruptId].count;
    const int newCount = oldCount + 1;
    InterruptHandler* pOldHandlers = pController->handlers[interruptId].start;
    InterruptHandler* pNewHandlers = NULL;
    
    try(kalloc(sizeof(InterruptHandler) * newCount, (void**) &pNewHandlers));

    
    // Allocate a new handler ID
    handlerId = pController->nextAvailableId;
    pController->nextAvailableId++;
    
    
    // Copy the old handlers over to the new array
    for (int i = 0; i < oldCount; i++) {
        pNewHandlers[i] = pOldHandlers[i];
    }
    pNewHandlers[oldCount] = *pHandler;
    pNewHandlers[oldCount].identity = handlerId;
    
    
    // Sort the handlers by priority
    insertion_sort(pNewHandlers, newCount);
    
    
    // Atomically (with respect to the IRQ handler for this CPU) install the new handlers
    const int sis = irq_disable();
    pController->handlers[interruptId].start = pNewHandlers;
    pController->handlers[interruptId].count = newCount;
    irq_restore(sis);
    
    
    // Enable the IRQ source
    if (newCount > 0) {
        chipset_enable_interrupt(interruptId);
    }
    
    
    // Free the old handler array
    kfree(pOldHandlers);
    
    *pOutId = handlerId;
    Lock_Unlock(&pController->lock);
    return EOK;

catch:
    if (needsUnlock) {
        Lock_Unlock(&pController->lock);
    }
    *pOutId = 0;
    return err;
}

// Registers a direct interrupt handler. The interrupt controller will invoke the
// given closure with the given context every time an interrupt with ID 'interruptId'
// is triggered.
// NOTE: The closure is invoked in the interrupt context.
errno_t InterruptController_AddDirectInterruptHandler(InterruptControllerRef _Nonnull pController, InterruptID interruptId, int priority, InterruptHandler_Closure _Nonnull pClosure, void* _Nullable pContext, InterruptHandlerID* _Nonnull pOutId)
{
    InterruptHandler handler;
    
    assert(pClosure != NULL);
    
    handler.identity = 0;
    handler.priority = __max(__min(priority, INTERRUPT_HANDLER_PRIORITY_HIGHEST), INTERRUPT_HANDLER_PRIORITY_LOWEST);
    handler.flags = 0;
    handler.type = INTERRUPT_HANDLER_TYPE_DIRECT;
    handler.closure = pClosure;
    handler.context = pContext;
    
    return InterruptController_AddInterruptHandler(pController, interruptId, &handler, pOutId);
}

// Registers a counting semaphore which will receive a release call for every
// occurrence of an interrupt with ID 'interruptId'.
errno_t InterruptController_AddSemaphoreInterruptHandler(InterruptControllerRef _Nonnull pController, InterruptID interruptId, int priority, Semaphore* _Nonnull pSemaphore, InterruptHandlerID* _Nonnull pOutId)
{
    InterruptHandler handler;
    
    assert(pSemaphore != NULL);
    
    handler.identity = 0;
    handler.priority = priority;
    handler.flags = 0;
    handler.type = INTERRUPT_HANDLER_TYPE_COUNTING_SEMAPHORE;
    handler.closure = (InterruptHandler_Closure) Semaphore_RelinquishFromInterruptContext;
    handler.context = pSemaphore;
    
    return InterruptController_AddInterruptHandler(pController, interruptId, &handler, pOutId);
}

// Removes the interrupt handler for the given handler ID. Does nothing if no
// such handler is registered.
errno_t InterruptController_RemoveInterruptHandler(InterruptControllerRef _Nonnull pController, InterruptHandlerID handlerId)
{
    decl_try_err();
    bool needsUnlock = false;

    if (handlerId == 0) {
        return EOK;
    }
    
    Lock_Lock(&pController->lock);
    needsUnlock = true;
    
    // Find out which interrupt ID this handler handles
    int interruptId = -1;
    for (int i = 0; i < INTERRUPT_ID_COUNT; i++) {
        for (int j = 0; j < pController->handlers[i].count; j++) {
            if (pController->handlers[i].start[j].identity == handlerId) {
                interruptId = i;
                break;
            }
        }
    }
    
    if (interruptId == -1) {
        Lock_Unlock(&pController->lock);
        return EOK;
    }
    
    
    // Allocate a new handler array
    const int oldCount = pController->handlers[interruptId].count;
    const int newCount = oldCount - 1;
    InterruptHandler* pOldHandlers = pController->handlers[interruptId].start;
    InterruptHandler* pNewHandlers;
    
    try(kalloc(sizeof(InterruptHandler) * newCount, (void**) &pNewHandlers));
    
    
    // Copy over the handlers that we want to retain
    for (int oldIdx = 0, newIdx = 0; oldIdx < oldCount; oldIdx++) {
        if (pOldHandlers[oldIdx].identity != handlerId) {
            pNewHandlers[newIdx++] = pOldHandlers[oldIdx];
        }
    }
    

    // Disable the IRQ source
    if (newCount == 0) {
        chipset_disable_interrupt(interruptId);
    }

    
    // Atomically (with respect to the IRQ handler for this CPU) install the new handlers
    const int sis = irq_disable();
    pController->handlers[interruptId].start = pNewHandlers;
    pController->handlers[interruptId].count = newCount;
    irq_restore(sis);
    
    
    // Free the old handler array
    kfree(pOldHandlers);
    
    Lock_Unlock(&pController->lock);
    return EOK;

catch:
    if (needsUnlock) {
        Lock_Unlock(&pController->lock);
    }
    return err;
}

// Returns the interrupt handler for the given interrupt handler ID.
// Must be called while holding the lock.
static InterruptHandler* _Nullable InterruptController_GetInterruptHandlerForID_Locked(InterruptControllerRef _Nonnull pController, InterruptHandlerID handlerId)
{
    for (int i = 0; i < INTERRUPT_ID_COUNT; i++) {
        register InterruptHandler* pHandlers = pController->handlers[i].start;
        register const int count = pController->handlers[i].count;

        for (int j = 0; j < count; j++) {
            if (pHandlers[j].identity == handlerId) {
                return &pHandlers[j];
            }
        }
    }
    
    return NULL;
}

// Enables / disables the interrupt handler with the given interrupt handler ID.
// Note that interrupt handlers are by default disabled (when you add them). You
// need to enable an interrupt handler before it is able to respond to interrupt
// requests. A disabled interrupt handler ignores interrupt requests.
void InterruptController_SetInterruptHandlerEnabled(InterruptControllerRef _Nonnull pController, InterruptHandlerID handlerId, bool enabled)
{
    Lock_Lock(&pController->lock);
    
    InterruptHandler* pHandler = InterruptController_GetInterruptHandlerForID_Locked(pController, handlerId);
    assert(pHandler != NULL);
    if (enabled) {
        pHandler->flags |= INTERRUPT_HANDLER_FLAG_ENABLED;
    } else {
        pHandler->flags &= ~INTERRUPT_HANDLER_FLAG_ENABLED;
    }

    Lock_Unlock(&pController->lock);
}

// Returns true if the given interrupt handler is enabled; false otherwise.
bool InterruptController_IsInterruptHandlerEnabled(InterruptControllerRef _Nonnull pController, InterruptHandlerID handlerId)
{
    Lock_Lock(&pController->lock);
    
    InterruptHandler* pHandler = InterruptController_GetInterruptHandlerForID_Locked(pController, handlerId);
    assert(pHandler != NULL);
    const bool enabled = (pHandler->flags & INTERRUPT_HANDLER_FLAG_ENABLED) != 0 ? true : false;
    
    Lock_Unlock(&pController->lock);
    return enabled;
}

#if 0
void InterruptController_Dump(InterruptControllerRef _Nonnull pController)
{
    Lock_Lock(&pController->lock);
    
    printf("InterruptController = {\n");
    for (int i = 0; i < INTERRUPT_ID_COUNT; i++) {
        register const InterruptHandler* pHandlers = pController->handlers[i].start;
        register const int count = pController->handlers[i].count;

        printf("  IRQ %d = {\n", i);
        for (int h = 0; h < count; h++) {
            switch (pHandlers[h].type) {
                case INTERRUPT_HANDLER_TYPE_DIRECT:
                    printf("    direct[%d, %d] = {0x%p, 0x%p},\n", pHandlers[h].identity, pHandlers[h].priority, pHandlers[h].closure, pHandlers[h].context);
                    break;

                case INTERRUPT_HANDLER_TYPE_COUNTING_SEMAPHORE:
                    printf("    sema[%d, %d] = {0x%p},\n", pHandlers[h].identity, pHandlers[h].priority, pHandlers[h].context);
                    break;
                    
                default:
                    abort();
            }
        }
        printf("  },\n");
    }
    printf("}\n");
    Lock_Unlock(&pController->lock);
}
#endif

// Returns the number of uninitialized interrupts that have happened since boot.
// An uninitialized interrupt is an interrupt request from a peripheral that does
// not have a IRQ vector number set up for the interrupt.
int InterruptController_GetUninitializedInterruptCount(InterruptControllerRef _Nonnull pController)
{
    return pController->uninitializedInterruptCount;
}

// Returns the number of spurious interrupts that have happened since boot. A
// spurious interrupt is an interrupt request that was not acknowledged by the
// hardware.
int InterruptController_GetSpuriousInterruptCount(InterruptControllerRef _Nonnull pController)
{
    return pController->spuriousInterruptCount;
}

// Returns the number of non=maskable interrupts that have happened since boot.
int InterruptController_GetNonMaskableInterruptCount(InterruptControllerRef _Nonnull pController)
{
    return pController->nonMaskableInterruptCount;
}

// Called by the low-level interrupt handler code. Invokes the interrupt handlers
// for the given interrupt
void InterruptController_OnInterrupt(InterruptHandlerArray* _Nonnull pArray)
{
    register const InterruptHandler* pCur = &pArray->start[0];
    register const InterruptHandler* pEnd = &pArray->start[pArray->count];

    while (pCur != pEnd) {
        if ((pCur->flags & INTERRUPT_HANDLER_FLAG_ENABLED) != 0) {
            pCur->closure(pCur->context);
        }

        pCur++;
    }
}
