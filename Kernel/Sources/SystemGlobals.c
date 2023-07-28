//
//  SystemGlobals.c
//  Apollo
//
//  Created by Dietmar Planitzer on 7/27/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "SystemGlobals.h"


// Kernel services
DispatchQueueRef        gMainDispatchQueue;
VirtualProcessorPoolRef gVirtualProcessorPool;
Console*                gConsole;
GraphicsDriverRef       gMainGDevice;
Heap*                   gHeap;
CopperScheduler         gCopperSchedulerStorage;


// Processes
ProcessRef          gRootProcess;


// Drivers
EventDriverRef      gEventDriver;
FloppyDMA*          gFloppyDma;
RealtimeClock*      gRealtimeClock;
