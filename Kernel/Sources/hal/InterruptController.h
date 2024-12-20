//
//  InterruptController.h
//  kernel
//
//  Created by Dietmar Planitzer on 5/5/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef InterruptController_h
#define InterruptController_h

#include <klib/klib.h>
#include <dispatcher/Semaphore.h>
#include <hal/Platform.h>


#define INTERRUPT_HANDLER_PRIORITY_LOWEST   -128
#define INTERRUPT_HANDLER_PRIORITY_NORMAL   0
#define INTERRUPT_HANDLER_PRIORITY_HIGHEST   127


// An interrupt ID
typedef int InterruptID;

// The ID that represents a specific registered interrupt handler
typedef int InterruptHandlerID;

// Closure which is invoked when an interrupt happens
typedef void (*InterruptHandler_Closure)(void* _Nullable pContext);


struct InterruptHandlerArray;
struct InterruptController;
typedef struct InterruptController* InterruptControllerRef;


// The shared interrupt controller instance
extern InterruptControllerRef _Nonnull  gInterruptController;

extern errno_t InterruptController_CreateForLocalCPU(void);


// Registers a direct interrupt handler. The interrupt controller will invoke the
// given closure with the given context every time an interrupt with ID 'interruptId'
// is triggered.
// NOTE: The closure is invoked in the interrupt context.
extern errno_t InterruptController_AddDirectInterruptHandler(InterruptControllerRef _Nonnull pController, InterruptID interruptId, int priority, InterruptHandler_Closure _Nonnull pClosure, void* _Nullable pContext, InterruptHandlerID* _Nonnull pOutId);

// Registers a counting semaphore which will receive a release call for every
// occurrence of an interrupt with ID 'interruptId'.
extern errno_t InterruptController_AddSemaphoreInterruptHandler(InterruptControllerRef _Nonnull pController, InterruptID interruptId, int priority, Semaphore* _Nonnull pSemaphore, InterruptHandlerID* _Nonnull pOutId);

// Removes the interrupt handler for the given handler ID. Does nothing if no
// such handler is registered.
extern errno_t InterruptController_RemoveInterruptHandler(InterruptControllerRef _Nonnull pController, InterruptHandlerID handlerId);

// Enables / disables the interrupt handler with the given interrupt handler ID.
// Note that interrupt handlers are by default disabled (when you add them). You
// need to enable an interrupt handler before it is able to respond to interrupt
// requests. A disabled interrupt handler ignores interrupt requests.
extern void InterruptController_SetInterruptHandlerEnabled(InterruptControllerRef _Nonnull pController, InterruptHandlerID handlerId, bool enabled);

// Returns true if the given interrupt handler is enabled; false otherwise.
extern bool InterruptController_IsInterruptHandlerEnabled(InterruptControllerRef _Nonnull pController, InterruptHandlerID handlerId);

// Called by the low-level interrupt handler code. Invokes the interrupt handlers
// for the given interrupt
extern void InterruptController_OnInterrupt(struct InterruptHandlerArray* _Nonnull pArray);

// Returns the number of uninitialized interrupts that have happened since boot.
// An uninitialized interrupt is an interrupt request from a peripheral that does
// not have a IRQ vector number set up for the interrupt.
extern int InterruptController_GetUninitializedInterruptCount(InterruptControllerRef _Nonnull pController);

// Returns the number of spurious interrupts that have happened since boot. A
// spurious interrupt is an interrupt request that was not acknowledged by the
// hardware.
extern int InterruptController_GetSpuriousInterruptCount(InterruptControllerRef _Nonnull pController);

// Returns the number of non=maskable interrupts that have happened since boot.
extern int InterruptController_GetNonMaskableInterruptCount(InterruptControllerRef _Nonnull pController);

extern void InterruptController_Dump(InterruptControllerRef _Nonnull pController);

#endif /* InterruptController_h */
