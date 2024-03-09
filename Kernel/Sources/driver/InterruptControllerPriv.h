//
//  InterruptControllerPriv.h
//  kernel
//
//  Created by Dietmar Planitzer on 7/28/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef InterruptControllerPriv_h
#define InterruptControllerPriv_h

#include "InterruptController.h"
#include <dispatcher/Lock.h>


#define INTERRUPT_HANDLER_TYPE_DIRECT               0
#define INTERRUPT_HANDLER_TYPE_COUNTING_SEMAPHORE   1

#define INTERRUPT_HANDLER_FLAG_ENABLED  0x01

// Keep this at a size that's a power-of-2
typedef struct _InterruptHandler {
    int                                 identity;
    int8_t                              type;
    int8_t                              priority;
    uint8_t                             flags;
    int8_t                              reserved;
    InterruptHandler_Closure _Nonnull   closure;
    void* _Nullable                     context;
} InterruptHandler;


// Keep in sync with lowmem.i
typedef struct _InterruptHandlerArray {
    InterruptHandler* _Nonnull  start;  // points to the first handler
    int                         count;
} InterruptHandlerArray;


// Keep in sync with lowmem.i
typedef struct _InterruptController {
    InterruptHandlerArray   handlers[INTERRUPT_ID_COUNT];
    int                     nextAvailableId;    // Next available interrupt handler ID
    int                     spuriousInterruptCount;
    int                     uninitializedInterruptCount;
    int                     nonMaskableInterruptCount;
    Lock                    lock;
} InterruptController;

#endif /* InterruptControllerPriv_h */
