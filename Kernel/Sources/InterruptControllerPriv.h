//
//  InterruptControllerPriv.h
//  Apollo
//
//  Created by Dietmar Planitzer on 7/28/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef InterruptControllerPriv_h
#define InterruptControllerPriv_h

#include "InterruptController.h"
#include "Lock.h"


#define INTERRUPT_HANDLER_TYPE_DIRECT               0
#define INTERRUPT_HANDLER_TYPE_COUNTING_SEMAPHORE   1

#define INTERRUPT_HANDLER_FLAG_ENABLED  0x01

// Keep this at a size that's a power-of-2
typedef struct _InterruptHandler {
    Int                                 identity;
    Int8                                type;
    Int8                                priority;
    UInt8                               flags;
    Int8                                reserved;
    InterruptHandler_Closure _Nonnull   closure;
    Byte* _Nullable                     context;
} InterruptHandler;


// Keep in sync with lowmem.i
typedef struct _InterruptHandlerArray {
    InterruptHandler* _Nonnull  start;  // points to the first handler
    Int                         count;
} InterruptHandlerArray;


// Keep in sync with lowmem.i
typedef struct _InterruptController {
    InterruptHandlerArray   handlers[INTERRUPT_ID_COUNT];
    Int                     nextAvailableId;    // Next available interrupt handler ID
    Int                     spuriousInterruptCount;
    Int                     uninitializedInterruptCount;
    Lock                    lock;
} InterruptController;

#endif /* InterruptControllerPriv_h */
