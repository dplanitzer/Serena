//
//  SystemGlobals.h
//  Apollo
//
//  Created by Dietmar Planitzer on 2/8/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef SystemGlobals_h
#define SystemGlobals_h

#include "Foundation.h"
#include "Atomic.h"
#include "Console.h"
#include "EventDriver.h"
#include "FloppyDisk.h"
#include "GraphicsDriver.h"
#include "Heap.h"
#include "Process.h"
#include "RealtimeClock.h"
#include "VirtualProcessorPool.h"


// Note: Keep in sync with lowmem.i
typedef struct _SystemGlobals {
    Heap* _Nonnull                      heap;                       // The kernel heap
    GraphicsDriverRef _Nonnull          main_screen_gdevice;        // Graphics device for the main screen
    Console* _Nullable                  console;
    RealtimeClock* _Nullable            rtc;                        // The realtime clock (if installed)
    FloppyDMA* _Nonnull                 floppy_dma;                 // Floppy DMA singleton
    volatile Int                        next_available_vpid;        // Next available virtual processor ID
    VirtualProcessorPoolRef _Nonnull    virtual_processor_pool;
    void* _Nonnull                      kernel_main_dispatch_queue;
    AtomicInt                           next_available_pid;
    ProcessRef _Nonnull                 root_process;
    EventDriverRef _Nonnull             event_driver;
} SystemGlobals;


extern SystemGlobals* _Nonnull SystemGlobals_Get(void);

#endif /* SystemGlobals_h */
