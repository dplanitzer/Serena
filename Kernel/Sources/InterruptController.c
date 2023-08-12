//
//  InterruptController.c
//  Apollo
//
//  Created by Dietmar Planitzer on 5/5/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "InterruptControllerPriv.h"
#include "kalloc.h"


InterruptController     gInterruptControllerStorage;
InterruptControllerRef  gInterruptController = &gInterruptControllerStorage;


ErrorCode InterruptController_CreateForLocalCPU(void)
{    
    decl_try_err();
    InterruptController* pController = &gInterruptControllerStorage;

    for (Int i = 0; i < INTERRUPT_ID_COUNT; i++) {
        try(kalloc(0, (Byte**) &pController->handlers[i].data));
        pController->handlers[i].size = 0;
    }
    
    pController->nextAvailableId = 1;
    pController->spuriousInterruptCount = 0;
    
    Lock_Init(&pController->lock);
    return EOK;

catch:
    return err;
}

static void insertion_sort(InterruptHandler* _Nonnull pHandlers, Int n)
{
    for (Int i = 1; i < n; i++) {
        const InterruptHandler handler = pHandlers[i];
        Int j = i - 1;
        
        while (j >= 0 && pHandlers[j].priority < handler.priority) {
            pHandlers[j + 1] = pHandlers[j];
            j = j - 1;
        }
        pHandlers[j + 1] = handler;
    }
}

// Adds the given interrupt handler to the controller. Returns the ID of the handler.
// 0 is returned if allocation failed.
static ErrorCode InterruptController_AddInterruptHandler(InterruptControllerRef _Nonnull pController, InterruptID interruptId, InterruptHandler* _Nonnull pHandler, InterruptHandlerID* _Nonnull pOutId)
{
    decl_try_err();
    InterruptHandlerID handlerId = 0;
    Bool needsUnlock = false;
    
    assert(pHandler->identity == 0);
    
    Lock_Lock(&pController->lock);
    needsUnlock = true;

    // Allocate a new handler array
    const Int oldSize = pController->handlers[interruptId].size;
    const Int newSize = oldSize + 1;
    InterruptHandler* pOldHandlers = pController->handlers[interruptId].data;
    InterruptHandler* pNewHandlers = NULL;
    
    try(kalloc(sizeof(InterruptHandler) * newSize, (Byte**) &pNewHandlers));

    
    // Allocate a new handler ID
    handlerId = pController->nextAvailableId;
    pController->nextAvailableId++;
    
    
    // Copy the old handlers over to the new array
    for (Int i = 0; i < oldSize; i++) {
        pNewHandlers[i] = pOldHandlers[i];
    }
    pNewHandlers[oldSize] = *pHandler;
    pNewHandlers[oldSize].identity = handlerId;
    
    
    // Sort the handlers by priority
    insertion_sort(pNewHandlers, newSize);
    
    
    // Atomically (with respect to the IRQ handler for this CPU) install the new handlers
    const Int sis = cpu_disable_irqs();
    pController->handlers[interruptId].data = pNewHandlers;
    pController->handlers[interruptId].size = newSize;
    cpu_restore_irqs(sis);
    
    
    // Enable the IRQ source
    if (newSize > 0) {
        chipset_enable_interrupt(interruptId);
    }
    
    
    // Free the old handler array
    kfree((Byte*) pOldHandlers);
    
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
ErrorCode InterruptController_AddDirectInterruptHandler(InterruptControllerRef _Nonnull pController, InterruptID interruptId, Int priority, InterruptHandler_Closure _Nonnull pClosure, Byte* _Nullable pContext, InterruptHandlerID* _Nonnull pOutId)
{
    InterruptHandler handler;
    
    assert(pClosure != NULL);
    
    handler.identity = 0;
    handler.priority = max(min(priority, INTERRUPT_HANDLER_PRIORITY_HIGHEST), INTERRUPT_HANDLER_PRIORITY_LOWEST);
    handler.flags = 0;
    handler.reserved = 0;
    handler.type = INTERRUPT_HANDLER_TYPE_DIRECT;
    handler.u.direct.closure = pClosure;
    handler.u.direct.context = pContext;
    
    return InterruptController_AddInterruptHandler(pController, interruptId, &handler, pOutId);
}

// Registers a counting semaphore which will receive a release call for every
// occurence of an interrupt with ID 'interruptId'.
ErrorCode InterruptController_AddSemaphoreInterruptHandler(InterruptControllerRef _Nonnull pController, InterruptID interruptId, Int priority, Semaphore* _Nonnull pSemaphore, InterruptHandlerID* _Nonnull pOutId)
{
    InterruptHandler handler;
    
    assert(pSemaphore != NULL);
    
    handler.identity = 0;
    handler.priority = priority;
    handler.flags = 0;
    handler.reserved = 0;
    handler.type = INTERRUPT_HANDLER_TYPE_COUNTING_SEMAPHORE;
    handler.u.sema.semaphore = pSemaphore;
    
    return InterruptController_AddInterruptHandler(pController, interruptId, &handler, pOutId);
}

// Removes the interrupt handler for the given handler ID. Does nothing if no
// such handler is registered.
ErrorCode InterruptController_RemoveInterruptHandler(InterruptControllerRef _Nonnull pController, InterruptHandlerID handlerId)
{
    decl_try_err();
    Bool needsUnlock = false;

    if (handlerId == 0) {
        return EOK;
    }
    
    Lock_Lock(&pController->lock);
    needsUnlock = true;
    
    // Find out which interrupt ID this handler handles
    Int interruptId = -1;
    for (Int i = 0; i < INTERRUPT_ID_COUNT; i++) {
        for (Int j = 0; j < pController->handlers[i].size; j++) {
            if (pController->handlers[i].data[j].identity == handlerId) {
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
    const Int oldSize = pController->handlers[interruptId].size;
    const Int newSize = oldSize - 1;
    InterruptHandler* pOldHandlers = pController->handlers[interruptId].data;
    InterruptHandler* pNewHandlers;
    
    try(kalloc(sizeof(InterruptHandler) * newSize, (Byte**) &pNewHandlers));
    
    
    // Copy over the handlers that we want to retain
    for (Int oldIdx = 0, newIdx = 0; oldIdx < oldSize; oldIdx++) {
        if (pOldHandlers[oldIdx].identity != handlerId) {
            pNewHandlers[newIdx++] = pOldHandlers[oldIdx];
        }
    }
    

    // Disable the IRQ source
    if (newSize == 0) {
        chipset_disable_interrupt(interruptId);
    }

    
    // Atomically (with respect to the IRQ handler for this CPU) install the new handlers
    const Int sis = cpu_disable_irqs();
    pController->handlers[interruptId].data = pNewHandlers;
    pController->handlers[interruptId].size = newSize;
    cpu_restore_irqs(sis);
    
    
    // Free the old handler array
    kfree((Byte*) pOldHandlers);
    
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
    for (Int i = 0; i < INTERRUPT_ID_COUNT; i++) {
        register InterruptHandler* pHandlers = pController->handlers[i].data;
        register const Int count = pController->handlers[i].size;

        for (Int j = 0; j < count; j++) {
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
void InterruptController_SetInterruptHandlerEnabled(InterruptControllerRef _Nonnull pController, InterruptHandlerID handlerId, Bool enabled)
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
Bool InterruptController_IsInterruptHandlerEnabled(InterruptControllerRef _Nonnull pController, InterruptHandlerID handlerId)
{
    Lock_Lock(&pController->lock);
    
    InterruptHandler* pHandler = InterruptController_GetInterruptHandlerForID_Locked(pController, handlerId);
    assert(pHandler != NULL);
    const Bool enabled = (pHandler->flags & INTERRUPT_HANDLER_FLAG_ENABLED) != 0 ? true : false;
    
    Lock_Unlock(&pController->lock);
    return enabled;
}

void InterruptController_Dump(InterruptControllerRef _Nonnull pController)
{
    Lock_Lock(&pController->lock);
    
    print("InterruptController = {\n");
    for (Int i = 0; i < INTERRUPT_ID_COUNT; i++) {
        register const InterruptHandler* pHandlers = pController->handlers[i].data;
        register const Int count = pController->handlers[i].size;

        print("  IRQ %d = {\n", i);
        for (Int h = 0; h < count; h++) {
            switch (pHandlers[h].type) {
                case INTERRUPT_HANDLER_TYPE_DIRECT:
                    print("    direct[%d, %d] = {0x%p, 0x%p},\n", pHandlers[h].identity, pHandlers[h].priority, pHandlers[h].u.direct.closure, pHandlers[h].u.direct.context);
                    break;

                case INTERRUPT_HANDLER_TYPE_COUNTING_SEMAPHORE:
                    print("    sema[%d, %d] = {0x%p},\n", pHandlers[h].identity, pHandlers[h].priority, pHandlers[h].u.sema.semaphore);
                    break;
                    
                default:
                    abort();
            }
        }
        print("  },\n");
    }
    print("}\n");
    Lock_Unlock(&pController->lock);
}

// Returns the number of spurious interrupts that have happened since boot. A
// spurious interrupt is an interrupt request that was not acknowledged by the
// hardware.
Int InterruptController_GetSpuriousInterruptCount(InterruptControllerRef _Nonnull pController)
{
    return pController->spuriousInterruptCount;
}

// Returns true if the caller is running in the interrupt context and false otherwise.
Bool InterruptController_IsServicingInterrupt(InterruptControllerRef _Nonnull pController)
{
    return pController->isServicingInterrupt;
}

// Called by the low-level interrupt handler code. Invokes the interrupt handlers
// for the given interrupt
void InterruptController_OnInterrupt(InterruptHandlerArray* _Nonnull pArray)
{
    for (register Int i = 0; i < pArray->size; i++) {
        register const InterruptHandler* pHandler = &pArray->data[i];
        
        if ((pHandler->flags & INTERRUPT_HANDLER_FLAG_ENABLED) != 0) {
            switch (pHandler->type) {
                case INTERRUPT_HANDLER_TYPE_DIRECT:
                    pHandler->u.direct.closure(pHandler->u.direct.context);
                    break;

                case INTERRUPT_HANDLER_TYPE_COUNTING_SEMAPHORE:
                    Semaphore_ReleaseMultiple(pHandler->u.sema.semaphore, 1);
                    break;
                    
                default:
                    abort();
            }
        }
    }
}
