//
//  startup.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/4/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include <krt/krt.h>
#include <klib/klib.h>
#include <disk/DiskCache.h>
#include <dispatcher/VirtualProcessorScheduler.h>
#include <dispatcher/VirtualProcessorPool.h>
#include <dispatchqueue/DispatchQueue.h>
#include <driver/DriverCatalog.h>
#include <driver/amiga/AmigaController.h>
#include <filesystem/Filesystem.h>
#include <hal/InterruptController.h>
#include <hal/MonotonicClock.h>
#include <hal/Platform.h>
#include <process/Process.h>
#include <process/ProcessManager.h>
#include "BootAllocator.h"

extern char _text, _etext, _data, _edata, _bss, _ebss;
static char* gInitialHeapBottom;
static char* gInitialHeapTop;


extern FilesystemRef _Nonnull create_boot_filesystem(void);
static _Noreturn OnStartup(const SystemDescription* _Nonnull pSysDesc);
static void OnMain(void);


// Called from the boot services at system reset time. Only a very minimal
// environment is set up at this point. IRQs and DMAs are off, CPU vectors are
// set up and a small reset stack exists. This function should kick off the
// kernel initialization by primarily setting up the kernel data and bss segments,
// basic memory management and the virtual boot processor. Note that this
// function is expected to never return.
_Noreturn OnBoot(SystemDescription* _Nonnull pSysDesc)
{
    const size_t data_size = &_edata - &_data;
    const size_t bss_size = &_ebss - &_bss;

    // Copy the kernel data segment from ROM to RAM
    memcpy(&_data, &_etext, data_size);

    // Initialize the BSS segment
    memset(&_bss, 0, bss_size);


    // Carve the kernel data and bss out from memory descriptor #0 to ensure that
    // our kernel heap is not going to try to override the data/bss region.
    gInitialHeapBottom = pSysDesc->motherboard_ram.descriptor[0].lower + __Ceil_PowerOf2(data_size + bss_size, CPU_PAGE_SIZE);
    pSysDesc->motherboard_ram.descriptor[0].lower = gInitialHeapBottom;


    // Store a reference to the system description in our globals
    gSystemDescription = pSysDesc;


    // Register all classes from the __class section
    RegisterClasses();


    // Create the boot allocator
    BootAllocator boot_alloc;
    BootAllocator_Init(&boot_alloc, pSysDesc);


    // Initialize the scheduler
    VirtualProcessorScheduler_CreateForLocalCPU(pSysDesc, &boot_alloc, (VoidFunc_1)OnStartup, pSysDesc);


    // Don't need the boot allocator anymore
    gInitialHeapTop = BootAllocator_GetLowestAllocatedAddress(&boot_alloc);
    BootAllocator_Deinit(&boot_alloc);


    // Do the first ever context switch over to the boot virtual processor
    // execution context.
    VirtualProcessorScheduler_SwitchToBootVirtualProcessor();
}

// Invoked by onBoot(). The code here runs in the boot virtual processor execution
// context. Interrupts and DMAs are still turned off.
//
// Phase 1 initialization is responsible for bringing up the interrupt handling,
// basic memory management, monotonic clock and the kernel main dispatch queue.
static _Noreturn OnStartup(const SystemDescription* _Nonnull pSysDesc)
{
    // Initialize the kernel heap
    try_bang(kalloc_init(pSysDesc, gInitialHeapBottom, gInitialHeapTop));
    

    // Initialize the interrupt controller
    try_bang(InterruptController_CreateForLocalCPU());

    
    // Initialize the monotonic clock
    try_bang(MonotonicClock_CreateForLocalCPU(pSysDesc));

    
    // Inform the scheduler that the heap exists now and that it should finish
    // its boot related initialization sequence
    try_bang(VirtualProcessorScheduler_FinishBoot(gVirtualProcessorScheduler));
    
    
    // Initialize the virtual processor pool
    try_bang(VirtualProcessorPool_Create(&gVirtualProcessorPool));
    
    
    // Initialize the dispatch queue services
    try_bang(DispatchQueue_Create(0, 1, kDispatchQoS_Interactive, 0, gVirtualProcessorPool, NULL, (DispatchQueueRef*)&gMainDispatchQueue));
    
    
    // Enable interrupts
    cpu_enable_irqs();

    
    // Continue the kernel startup on the kernel main queue
    DispatchQueue_DispatchAsync(gMainDispatchQueue, (VoidFunc_1)OnMain, NULL);

    
    // The boot virtual processor now takes over the duties of running the
    // virtual processor scheduler service tasks.
    VirtualProcessorScheduler_Run(gVirtualProcessorScheduler);
}

// Creates and starts the platform controller which in turn discovers all platform
// specific drivers and gets them up and running.
static errno_t init_platform_controller(void)
{
    static PlatformControllerRef gPlatformController;
    decl_try_err();

    err = AmigaController_Create(&gPlatformController);

    if (err == EOK) {
        err = Driver_Start((DriverRef)gPlatformController);
    }
    return err;
}

// Called by the boot virtual processor after it has finished initializing all
// dispatch queue related services.
//
// This is the kernel main entry point which is responsible for bringing up the
// driver manager and the first process.
static void OnMain(void)
{
    decl_try_err();

    // Create the driver catalog
    try_bang(DriverCatalog_Create(&gDriverCatalog));

    // Create the platform controller
    try_bang(init_platform_controller());


    // Initialize the Kernel Runtime Services so that we can make it available
    // to userspace in the form of the Userspace Runtime Services.
    krt_init();
    

    // Create the disk cache
    try(DiskCache_Create(gSystemDescription, &gDiskCache));


    // Find and mount a root filesystem.
    FilesystemRef bootFS = create_boot_filesystem();


    // Create the root process
    ProcessRef pRootProc;
    try(RootProcess_Create(bootFS, &pRootProc));
    Object_Release(bootFS);


    // Create the process manager
    try(ProcessManager_Create(pRootProc, &gProcessManager));


    // Get the root process going
    print("Starting login...\n");
    try(RootProcess_Exec(pRootProc, "/System/Commands/login"));
    return;
    
catch:
    print("Error: unable to complete startup: %d\nHalting.\n", err);
    while(1);
    /* NOT REACHED */
}
