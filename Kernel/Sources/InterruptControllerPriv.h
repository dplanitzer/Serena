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

// What the interrupt controller should do with a handler at IRQ time
#define OPCODE_EXEC_DIRECT              0
#define OPCODE_POST_COUNTING_SEMAPHORE  1
#define OPCODE_NOP                      2


// Keep this at a size that's a power-of-2
typedef struct _InterruptHandler {
    Int     identity;
    Int8    type;
    Int8    priority;
    UInt8   flags;
    Int8    opcode;
    union {
        struct {
            InterruptHandler_Closure _Nonnull   closure;
            Byte* _Nullable                     context;
        }   direct;
        struct {
            Semaphore* _Nonnull semaphore;
        }   sema;
    }       u;
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
    Int8                    isServicingInterrupt;   // > 0 while we are running in the IRQ context; == 0 if we are running outside the IRQ context
    Int8                    reserved[3];
    Lock                    lock;
} InterruptController;

#endif /* InterruptControllerPriv_h */
