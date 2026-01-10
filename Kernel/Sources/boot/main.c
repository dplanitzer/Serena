//
//  main.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/4/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include <string.h>
#include <console/Console.h>
#include <diskcache/DiskCache.h>
#include <driver/DriverManager.h>
#include <driver/hid/HIDManager.h>
#include <filemanager/FilesystemManager.h>
#include <filesystem/Filesystem.h>
#include <filesystem/kernfs/KernFS.h>
#include <hal/clock.h>
#include <hal/irq.h>
#include <hal/sched.h>
#include <kei/kei.h>
#include <kern/log.h>
#include <process/Process.h>
#include <process/ProcessManager.h>
#include <sched/delay.h>
#include <sched/sched.h>
#include <sched/vcpu_pool.h>
#include <security/SecurityManager.h>
#include <sys/mount.h>
#include <Catalog.h>
#include <kern/kalloc.h>
#include "BootAllocator.h"
#include "boot_screen.h"

extern char _text, _etext, _data, _edata, _bss, _ebss;

extern errno_t kerneld_init(void);
extern errno_t drivers_init(void);
extern FileHierarchyRef _Nonnull create_root_file_hierarchy(bt_screen_t* _Nonnull bscr);
static _Noreturn OnStartup(const sys_desc_t* _Nonnull pSysDesc);
static void OnMain(void);


static char* gInitialHeapBottom;
static char* gInitialHeapTop;

static bt_screen_t gBootScreen;
static ConsoleRef gConsole;


// Called from the boot services at system reset time. Only a very minimal
// environment is set up at this point. IRQs and DMAs are off, CPU vectors are
// set up and a small reset stack exists. This function should kick off the
// kernel initialization by primarily setting up the kernel data and bss segments,
// basic memory management and the virtual boot processor. Note that this
// function is expected to never return.
_Noreturn OnBoot(sys_desc_t* _Nonnull pSysDesc)
{
    const size_t data_size = &_edata - &_data;
    const size_t bss_size = &_ebss - &_bss;

    // Copy the kernel data segment from ROM to RAM
    memcpy(&_data, &_etext, data_size);

    // Initialize the BSS segment
    memset(&_bss, 0, bss_size);


    // Carve the kernel data and bss out from memory descriptor #0 to ensure that
    // our kernel heap is not going to try to override the data/bss region.
    gInitialHeapBottom = pSysDesc->motherboard_ram.desc[0].lower + __Ceil_PowerOf2(data_size + bss_size, CPU_PAGE_SIZE);
    pSysDesc->motherboard_ram.desc[0].lower = gInitialHeapBottom;


    // Store a reference to the system description in our globals
    g_sys_desc = pSysDesc;


    // Register all classes from the __class section
    RegisterClasses();


    // Create the boot allocator
    BootAllocator boot_alloc;
    BootAllocator_Init(&boot_alloc, pSysDesc);


    // Initialize the scheduler
    sched_create(&boot_alloc, pSysDesc, (VoidFunc_1)OnStartup, pSysDesc);


    // Don't need the boot allocator anymore
    gInitialHeapTop = BootAllocator_GetLowestAllocatedAddress(&boot_alloc);
    BootAllocator_Deinit(&boot_alloc);


    // Do the first ever context switch over to the boot virtual processor
    // execution context.
    sched_switch_to_boot_vcpu();
}

// Invoked by onBoot(). The code here runs in the boot virtual processor execution
// context. Interrupts and DMAs are still turned off.
//
// Phase 1 initialization is responsible for bringing up the interrupt handling,
// basic memory management, monotonic clock and the kernel main dispatch queue.
static _Noreturn OnStartup(const sys_desc_t* _Nonnull pSysDesc)
{
    decl_try_err();

    // Initialize the kernel heap
    try_bang(kalloc_init(pSysDesc, gInitialHeapBottom, gInitialHeapTop));

    
    // Initialize the kernel delay service
    delay_init();


    // Initialize the monotonic clock
    clock_init_mono(g_mono_clock);

    
    // Initialize the virtual processor pool
    try_bang(vcpu_pool_create(&g_vcpu_pool));
    

    // Start the monotonic clock
    clock_start(g_mono_clock);


    // Enable interrupt processing
    irq_restore_mask(IRQ_MASK_NONE);

    
    // Initialize the kernel logging package 
    log_init();


    // Create the security manager
    try(SecurityManager_Create(&gSecurityManager));


    // Create the process manager
    try(ProcessManager_Create(&gProcessManager));


    // Create the filesystem manager
    try(FilesystemManager_Create(&gFilesystemManager));


    // Create the disk cache
    try(DiskCache_Create(512, sys_desc_getramsize(pSysDesc) >> 5, &gDiskCache));


    // Create the various kernel object catalogs
    try(Catalog_Create(&gFSCatalog));
    try(Catalog_Create(&gProcCatalog));
    try(Catalog_Create(&gDriverCatalog));
    try(Filesystem_Publish(Catalog_GetFilesystem(gFSCatalog)));
    try(Filesystem_Publish(Catalog_GetFilesystem(gProcCatalog)));
    try(Filesystem_Publish(Catalog_GetFilesystem(gDriverCatalog)));


    // Create the HID and driver managers
    try(HIDManager_Create(&gHIDManager));
    try(DriverManager_Create(&gDriverManager));
    

    // Create the kerneld process and publish it
    try(kerneld_init());


    // Detect hardware and initialize boot-time drivers
    try(drivers_init());


    // Start the HID services
    try(HIDManager_Start(gHIDManager));


    // Open the boot screen and show the boot logo
    bt_open(&gBootScreen);


    // Create the root file hierarchy
    FileHierarchyRef pRootFh = create_root_file_hierarchy(&gBootScreen);

    
    // Start the filesystem management services
    try(FilesystemManager_Start(gFilesystemManager));


    // Initialize the Kernel Runtime Services so that we can make it available
    // to userspace in the form of the Userspace Runtime Services.
    kei_init();


    // Spawn systemd
    try(KernelProcess_SpawnSystemd(gKernelProcess, pRootFh));
    Object_Release(pRootFh);

    
    // The boot virtual processor now takes over the duties of running the
    // virtual processor scheduler service tasks.
    sched_run_chores(g_sched);

catch:
    irq_set_mask(IRQ_MASK_ALL);
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

    bt_close(&gBootScreen);

    // Initialize the console
    try(Console_Create(&gConsole));
    try(Driver_Start((DriverRef)gConsole));

    log_switch_to_console();

catch:
    return err;
}
