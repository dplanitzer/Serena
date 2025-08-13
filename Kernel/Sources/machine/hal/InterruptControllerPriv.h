//
//  InterruptControllerPriv.h
//  kernel
//
//  Created by Dietmar Planitzer on 7/28/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef InterruptControllerPriv_h
#define InterruptControllerPriv_h

#include <machine/InterruptController.h>
#include <machine/irq.h>
#include <sched/mtx.h>


#define INTERRUPT_HANDLER_TYPE_DIRECT               0
#define INTERRUPT_HANDLER_TYPE_COUNTING_SEMAPHORE   1

#define INTERRUPT_HANDLER_FLAG_ENABLED  0x01

// Keep this at a size that's a power-of-2
typedef struct InterruptHandler {
    int                                 identity;
    int8_t                              type;
    int8_t                              priority;
    uint8_t                             flags;
    int8_t                              reserved;
    InterruptHandler_Closure _Nonnull   closure;
    void* _Nullable                     context;
} InterruptHandler;


// Keep in sync with machine/hal/lowmem.i
typedef struct InterruptHandlerArray {
    InterruptHandler* _Nonnull  start;  // points to the first handler
    int                         count;
} InterruptHandlerArray;


// Keep in sync with machine/hal/lowmem.i
typedef struct InterruptController {
    InterruptHandlerArray   handlers[IRQ_ID_COUNT];
    int                     nextAvailableId;    // Next available interrupt handler ID
    int                     spuriousInterruptCount;
    int                     uninitializedInterruptCount;
    int                     nonMaskableInterruptCount;
    mtx_t                   mtx;
} InterruptController;

#endif /* InterruptControllerPriv_h */
