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
#include "Lock.h"
#include "Process.h"
#include "RealtimeClock.h"
#include "VirtualProcessorPool.h"
#include "DispatchQueue.h"


// Kernel services
extern DispatchQueueRef _Nonnull        gMainDispatchQueue;     // This is a serial queue
extern VirtualProcessorPoolRef _Nonnull gVirtualProcessorPool;
extern Console* _Nullable               gConsole;
extern GraphicsDriverRef _Nonnull       gMainGDevice;           // Graphics device for the main screen
extern Heap* _Nonnull                   gHeap;                  // The kernel heap


// Processes
extern ProcessRef _Nonnull          gRootProcess;


// Drivers
extern EventDriverRef _Nonnull      gEventDriver;
extern FloppyDMA* _Nonnull          gFloppyDma;                 // Floppy DMA singleton
extern RealtimeClock* _Nullable     gRealtimeClock;             // The realtime clock (if installed)

#endif /* SystemGlobals_h */
