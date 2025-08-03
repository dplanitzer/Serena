//
//  main.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/4/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include <kei/kei.h>
#include <log/Log.h>
#include <console/Console.h>
#include <diskcache/DiskCache.h>
#include <dispatchqueue/DispatchQueue.h>
#include <driver/DriverManager.h>
#include <driver/LogDriver.h>
#include <driver/NullDriver.h>
#include <driver/disk/VirtualDiskManager.h>
#include <driver/hid/HIDDriver.h>
#include <driver/hid/HIDManager.h>
#include <filemanager/FilesystemManager.h>
#include <filesystem/Filesystem.h>
#include <machine/amiga/AmigaController.h>
#include <machine/clock.h>
#include <machine/csw.h>
#include <machine/InterruptController.h>
#include <machine/Platform.h>
#include <process/Process.h>
#include <process/ProcessManager.h>
#include <sched/delay.h>
#include <sched/sched.h>
#include <sched/vcpu_pool.h>
#include <security/SecurityManager.h>
#include <sys/mount.h>
#include <Catalog.h>
#include <kern/kalloc.h>
#include <kern/string.h>
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
    sched_create(pSysDesc, &boot_alloc, (VoidFunc_1)OnStartup, pSysDesc);


    // Don't need the boot allocator anymore
    gInitialHeapTop = BootAllocator_GetLowestAllocatedAddress(&boot_alloc);
    BootAllocator_Deinit(&boot_alloc);


    // Do the first ever context switch over to the boot virtual processor
    // execution context.
    csw_switch_to_boot_vcpu();
}

static errno_t drivers_init(void)
{
    decl_try_err();

    // Platform controller
    try(PlatformController_Create(class(AmigaController), (DriverRef*)&gPlatformController));
    try(Driver_Start((DriverRef)gPlatformController));


    // 'hid' driver
    DriverRef hidDriver;
    try(HIDDriver_Create(&hidDriver));
    try(Driver_Start(hidDriver));


    // 'klog' driver
    DriverRef logDriver;
    try(LogDriver_Create(&logDriver));
    try(Driver_Start(logDriver));

        
    // 'null' driver
    DriverRef nullDriver;
    try(NullDriver_Create(&nullDriver));
    try(Driver_Start(nullDriver));


    // 'vdm' driver
    try(VirtualDiskManager_Create((DriverRef*)&gVirtualDiskManager));
    try(Driver_Start((DriverRef)gVirtualDiskManager));

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

    
    // Initialize the kernel delay service
    delay_init();


    // Initialize the interrupt controller
    try_bang(InterruptController_CreateForLocalCPU());

    
    // Initialize the monotonic clock
    clock_init_mono(g_mono_clock, pSysDesc);

    
    // Inform the scheduler that the heap exists now and that it should finish
    // its boot related initialization sequence
    sched_finish_boot(g_sched);
    
    
    // Initialize the virtual processor pool
    try_bang(vcpu_pool_create(&g_vcpu_pool));
    
    
    // Enable interrupts and the monotonic clock
    irq_enable();
    clock_enable(g_mono_clock);

    
    // Initialize the kernel logging package 
    log_init();


    // Create the security manager
    try(SecurityManager_Create(&gSecurityManager));


    // Create the process manager
    try(ProcessManager_Create(&gProcessManager));


    // Create the filesystem manager
    try(FilesystemManager_Create(&gFilesystemManager));


    // Create the disk cache
    try(DiskCache_Create(512, SystemDescription_GetRamSize(pSysDesc) >> 5, &gDiskCache));


    // Create the various kernel object catalogs
    try(Catalog_Create(&gFSCatalog));
    try(Catalog_Create(&gProcCatalog));
    try(Catalog_Create(&gDriverCatalog));
    try(Filesystem_Publish(Catalog_GetFilesystem(gFSCatalog)));
    try(Filesystem_Publish(Catalog_GetFilesystem(gProcCatalog)));
    try(Filesystem_Publish(Catalog_GetFilesystem(gDriverCatalog)));


    // Detect hardware and initialize drivers
    try(HIDManager_Create(&gHIDManager));
    try(DriverManager_Create(&gDriverManager));
    try(drivers_init());
    try(HIDManager_Start(gHIDManager));


    // Open the boot screen and show the boot logo
    open_boot_screen(&gBootScreen);
    
    
    // Create the root file hierarchy
    FileHierarchyRef pRootFh = create_root_file_hierarchy(&gBootScreen);


    // Create the kerneld process and publish it
    KernelProcess_Init(pRootFh, &gKernelProcess);
    try(ProcessManager_Publish(gProcessManager, gKernelProcess));
    Object_Release(pRootFh);


    // Start the filesystem management services
    try(FilesystemManager_Start(gFilesystemManager));


    // Initialize the Kernel Runtime Services so that we can make it available
    // to userspace in the form of the Userspace Runtime Services.
    kei_init();


    // Spawn systemd
    try(KernelProcess_SpawnSystemd(gKernelProcess));

    
    // The boot virtual processor now takes over the duties of running the
    // virtual processor scheduler service tasks.
    sched_run_chores(g_sched);

catch:
    irq_disable();
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
    log_switch_to_console();

catch:
    return err;
}
