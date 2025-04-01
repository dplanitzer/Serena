//
//  main.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/4/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include <krt/krt.h>
#include <klib/klib.h>
#include <log/Log.h>
#include <console/Console.h>
#include <diskcache/DiskCache.h>
#include <dispatcher/VirtualProcessorScheduler.h>
#include <dispatcher/VirtualProcessorPool.h>
#include <dispatchqueue/DispatchQueue.h>
#include <driver/DriverCatalog.h>
#include <driver/DriverChannel.h>
#include <driver/LogDriver.h>
#include <driver/NullDriver.h>
#include <driver/amiga/AmigaController.h>
#include <driver/amiga/graphics/GraphicsDriver.h>
#include <driver/hid/HIDDriver.h>
#include <driver/hid/HIDManager.h>
#include <filemanager/FilesystemManager.h>
#include <filesystem/Filesystem.h>
#include <hal/InterruptController.h>
#include <hal/MonotonicClock.h>
#include <hal/Platform.h>
#include <process/Process.h>
#include <process/ProcessManager.h>
#include <security/SecurityManager.h>
#include "BootAllocator.h"
#include "boot_screen.h"

extern char _text, _etext, _data, _edata, _bss, _ebss;
static char* gInitialHeapBottom;
static char* gInitialHeapTop;

boot_screen_t gBootScreen;
static ConsoleRef gConsole;


extern FileHierarchyRef _Nonnull create_root_file_hierarchy(boot_screen_t* _Nonnull bscr);
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


// Creates and starts the platform controller which in turn discovers all platform
// specific drivers and gets them up and running.
static errno_t drivers_init(void)
{
    static DriverRef gHidDriver;
    static DriverRef gLogDriver;
    static DriverRef gNullDriver;
    decl_try_err();

    // HID manager & driver
    try(HIDManager_Create(&gHIDManager));
    try(HIDDriver_Create(&gHidDriver));
    try(Driver_Start(gHidDriver));


    // Platform controller
    try(PlatformController_Create(class(AmigaController), &gPlatformController));
    try(Driver_Start(gPlatformController));


    // Start the HID manager
    try(HIDManager_Start(gHIDManager));


    // 'klog' driver
    try(LogDriver_Create(&gLogDriver));
    try(Driver_Start(gLogDriver));

        
    // 'null' driver
    try(NullDriver_Create(&gNullDriver));
    try(Driver_Start(gNullDriver));

catch:
    return err;
}

// Invoked by onBoot(). The code here runs in the boot virtual processor execution
// context. Interrupts and DMAs are still turned off.
//
// Phase 1 initialization is responsible for bringing up the interrupt handling,
// basic memory management, monotonic clock and the kernel main dispatch queue.
static _Noreturn OnStartup(const SystemDescription* _Nonnull pSysDesc)
{
    decl_try_err();

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
    
    
    // Enable interrupts
    cpu_enable_irqs();

    
    // Initialize the kernel logging package 
    log_init();


    // Create the driver catalog
    try_bang(DriverCatalog_Create(&gDriverCatalog));


    // Create the disk cache
    try(DiskCache_Create(gSystemDescription, &gDiskCache));


    // Create the security manager
    try(SecurityManager_Create(&gSecurityManager));

    
    // Detect hardware and initialize drivers
    try_bang(drivers_init());


    // Open the boot screen and show the boot logo
    open_boot_screen(&gBootScreen);


    // Initialize the Kernel Runtime Services so that we can make it available
    // to userspace in the form of the Userspace Runtime Services.
    krt_init();
    

    // Create the filesystem manager
    try(FilesystemManager_Create(&gFilesystemManager));

    
    // Create the root file hierarchy and process.
    FileHierarchyRef pRootFh = create_root_file_hierarchy(&gBootScreen);
    ProcessRef pRootProc;
    try(RootProcess_Create(pRootFh, &pRootProc));
    Object_Release(pRootFh);


    // Create the process manager
    try(ProcessManager_Create(pRootProc, &gProcessManager));


    // Get the root process going
    try(RootProcess_Exec(pRootProc, "/System/Commands/systemd"));

    
    // The boot virtual processor now takes over the duties of running the
    // virtual processor scheduler service tasks.
    VirtualProcessorScheduler_Run(gVirtualProcessorScheduler);

catch:
    printf("Error: unable to complete startup: %d\nHalting.\n", err);
    while(1);
    /* NOT REACHED */
}

// Invoked via system call by login. Shut down the boot screen and initialize
// the VT100 console.
// XXX This is a temporary solution until the VT100 console has been moved to
// XXX user space.
errno_t SwitchToFullConsole(void)
{
    decl_try_err();

    close_boot_screen(&gBootScreen);

    // Initialize the console
    try(Console_Create(&gConsole));
    try(Driver_Start((DriverRef)gConsole));

catch:
    return err;
}
