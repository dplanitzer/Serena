//
//  startup.c
//  Apollo
//
//  Created by Dietmar Planitzer on 2/4/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "Foundation.h"
#include "Console.h"
#include "DispatchQueue.h"
#include "EventDriver.h"
#include "Heap.h"
#include "InterruptController.h"
#include "MonotonicClock.h"
#include "Platform.h"
#include "Process.h"
#include "SystemGlobals.h"
#include "VirtualProcessorScheduler.h"
#include "VirtualProcessorPool.h"


static _Noreturn OnStartup_Phase1(const SystemDescription* _Nonnull pSysDesc);
static void OnStartup_Phase2(const SystemDescription* _Nonnull pSysDesc);


// Called from the Reset() function at system reset time. Only a very minimal
// environment is set up at this point. IRQs and DMAs are off, CPU vectors are
// set up and a small reset stack exists.
// OnReset() must finish the initialization of the provided system description.
// The 'stack_base' and 'stack_size' fields are set up with the reset stack. All
// other fields must be initialized before OnReset() returns.
void OnReset(SystemDescription* _Nonnull pSysDesc)
{
    SystemDescription_Init(pSysDesc);
}


// Find a suitable memory region for the boot kernel stack. We try to allocate
// the stack region from fast RAM and we try to put it as far up in RAM as
// possible. We only allocate from chip RAM if there is no fast RAM.
static Byte* _Nullable boot_allocate_kernel_stack(SystemDescription* _Nonnull pSysDesc, Int stackSize)
{
    Byte* pStackBase = NULL;
    
    for (Int i = pSysDesc->memory_descriptor_count - 1; i >= 0; i--) {
        const Int nAvailBytes = pSysDesc->memory_descriptor[i].upper - pSysDesc->memory_descriptor[i].lower;
        
        if (nAvailBytes >= stackSize) {
            pStackBase = pSysDesc->memory_descriptor[i].upper - stackSize;
            break;
        }
    }

    return pStackBase;
}

// Called by Reset() after OnReset() has returned. This function should initialize
// the boot virtual processor and the scheduler. Reset() will then do a single
// context switch to the boot virtual processor which will then continue with the
// initialization of the kernel.
void OnBoot(SystemDescription* _Nonnull pSysDesc)
{
    SystemGlobals* pGlobals = SystemGlobals_Get();
    
    // The first valid VPID is #1
    pGlobals->next_available_vpid = 1;
    
    
    // Allocate the kernel stack in high mem
    const Int kernelStackSize = VP_DEFAULT_KERNEL_STACK_SIZE;
    Byte* pKernelStackBase = boot_allocate_kernel_stack(pSysDesc, kernelStackSize);
    assert(pKernelStackBase != NULL);


    // Initialize the boot virtual processor
    VirtualProcessor* pVP = BootVirtualProcessor_GetShared();
    BootVirtualProcessor_Init(pVP, pSysDesc, VirtualProcessorClosure_MakeWithPreallocatedKernelStack((Closure1Arg_Func)OnStartup_Phase1, (Byte*)pSysDesc, pKernelStackBase, kernelStackSize));

    
    // Initialize the scheduler
    VirtualProcessorScheduler_Init(VirtualProcessorScheduler_GetShared(), pSysDesc, pVP);
}

// Called by the boot virtual processor after onBoot() has returned. This is the thread
// of execution that was started by the Reset() function.
//
// Phase 1 initialization is responsible for bringing up the interrupt handling,
// basic memort management, monotonic clock and the kernel main dispatch queue.
static _Noreturn OnStartup_Phase1(const SystemDescription* _Nonnull pSysDesc)
{
    SystemGlobals* pGlobals = SystemGlobals_Get();

    // Initialize the global heap
    assert(Heap_Create(pSysDesc->memory_descriptor, pSysDesc->memory_descriptor_count, &pGlobals->heap) == EOK);
    

    // Initialize the interrupt controller
    InterruptController_Init(InterruptController_GetShared());

    
    // Initialize the monotonic clock
    MonotonicClock_Init(MonotonicClock_GetShared(), pSysDesc);

    
    // Inform the scheduler that the heap exists now and that it should finish
    // its boot related initialization sequence
    VirtualProcessorScheduler_FinishBoot(VirtualProcessorScheduler_GetShared());
    
    
    // Initialize the virtual processor pool
    pGlobals->virtual_processor_pool = VirtualProcessorPool_Create();
    assert(pGlobals->virtual_processor_pool != NULL);
    
    
    // Initialize the dispatch queue services
    pGlobals->kernel_main_dispatch_queue = DispatchQueue_Create(1, DISPATCH_QOS_INTERACTIVE, 0, NULL);
    assert(pGlobals->kernel_main_dispatch_queue != NULL);
    
    
    // Enable interrupt handling
    cpu_enable_irqs();

    
    // Continue the kernel startup on the kernel main queue
    DispatchQueue_DispatchAsync(DispatchQueue_GetMain(), DispatchQueueClosure_Make((Closure1Arg_Func)OnStartup_Phase2, (Byte*)pSysDesc));

    
    // The boot virtual processor now takes over the duties of running the
    // virtual processor scheduler service tasks.
    VirtualProcessorScheduler_Run(VirtualProcessorScheduler_GetShared());
}

// Called by the boot virtual processor after it has finished initializing all
// dispatch queue related services.
//
// Phase 2 initialization is responsible for bringing up the device manager and
// the first process.
static void OnStartup_Phase2(const SystemDescription* _Nonnull pSysDesc)
{
    SystemGlobals* pGlobals = SystemGlobals_Get();

    // Initialize I/O services needed by the console
    pGlobals->rtc = RealtimeClock_Create(pSysDesc);
    
    const VideoConfiguration* pVideoConfig;
    if (chipset_is_ntsc()) {
        pVideoConfig = &kVideoConfig_NTSC_640_200_60;
    } else {
        pVideoConfig = &kVideoConfig_PAL_640_256_50;
    }
    
    pGlobals->main_screen_gdevice = GraphicsDriver_Create(pVideoConfig, kPixelFormat_RGB_Indexed1);
    assert(pGlobals->main_screen_gdevice != NULL);
    
    
    // Initialize the console
    pGlobals->console = Console_Create(pGlobals->main_screen_gdevice);
    assert(pGlobals->console != NULL);

    
    // Initialize all other I/O dervices
    pGlobals->floppy_dma = FloppyDMA_Create();
    assert(pGlobals->floppy_dma != NULL);

    pGlobals->event_driver = EventDriver_Create(pGlobals->main_screen_gdevice);
    assert(pGlobals->event_driver != NULL);
        

#if 1
    // Create the root process and kick it off running
    pGlobals->root_process = Process_Create(Process_GetNextAvailablePID());
    assert(pGlobals->root_process);

    Process_DispatchAsyncUser(pGlobals->root_process, (Closure1Arg_Func)0xfe0000);
#else
    // XXX Unit tests
    void DispatchQueue_RunTests(void);

    //InterruptController_Dump(InterruptController_GetShared());
    DispatchQueue_RunTests();
    // XXX
#endif
}
