//
//  startup.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/4/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include <krt/krt.h>
#include <klib/klib.h>
#include <dispatcher/VirtualProcessorScheduler.h>
#include <dispatcher/VirtualProcessorPool.h>
#include <dispatchqueue/DispatchQueue.h>
#include <driver/DriverManager.h>
#include <driver/InterruptController.h>
#include <driver/MonotonicClock.h>
#include <driver/RamDisk.h>
#include <driver/RomDisk.h>
#include <filesystem/FilesystemManager.h>
#include <filesystem/serenafs/SerenaFS.h>
#include <hal/Platform.h>
#include <process/Process.h>
#include <process/ProcessManager.h>
#include "BootAllocator.h"

extern char _text, _etext, _data, _edata, _bss, _ebss;
static char* gInitialHeapBottom;
static char* gInitialHeapTop;


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
    const int data_size = &_edata - &_data;
    const int bss_size = &_ebss - &_bss;

    // Copy the kernel data segment from ROM to RAM
    Bytes_CopyRange(&_data, &_etext, data_size);

    // Initialize the BSS segment
    Bytes_ClearRange(&_bss, bss_size);


    // Carve the kernel data and bss out from memory descriptor #0 to ensure that
    // our kernel heap is not going to try to override the data/bss region.
    gInitialHeapBottom = pSysDesc->memory.descriptor[0].lower + __Ceil_PowerOf2(data_size + bss_size, CPU_PAGE_SIZE);
    pSysDesc->memory.descriptor[0].lower = gInitialHeapBottom;


    // Store a reference to the system description in our globals
    gSystemDescription = pSysDesc;


    // Register all classes from the __class section
    RegisterClasses();


    // Create the boot allocator
    BootAllocator boot_alloc;
    BootAllocator_Init(&boot_alloc, pSysDesc);


    // Initialize the scheduler
    VirtualProcessorScheduler_CreateForLocalCPU(pSysDesc, &boot_alloc, (Closure1Arg_Func)OnStartup, pSysDesc);


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
    try_bang(DispatchQueue_Create(0, 1, kDispatchQos_Interactive, 0, gVirtualProcessorPool, NULL, (DispatchQueueRef*)&gMainDispatchQueue));
    
    
    // Enable interrupts
    cpu_enable_irqs();

    
    // Continue the kernel startup on the kernel main queue
    DispatchQueue_DispatchAsync(gMainDispatchQueue, DispatchQueueClosure_Make((Closure1Arg_Func)OnMain, NULL));

    
    // The boot virtual processor now takes over the duties of running the
    // virtual processor scheduler service tasks.
    VirtualProcessorScheduler_Run(gVirtualProcessorScheduler);
}

// Locates the root filesystem and mounts it.
static void init_root_filesystem(void)
{
    decl_try_err();
    RamDiskRef pRamDisk;
    FilesystemRef pFS;

    // XXX This is temporary:
    // XXX We're creating a RAM disk and then we'll look for a disk image in the
    // XXX ROM. We then copy this disk image into the RAM disk and use this as
    // XXX our root filesystem.

    // XXX We're looking at a fixed locations in the ROM for now.
    const char* p0 = (char*)(BOOT_ROM_BASE + SIZE_KB(128));
    const char* p1 = (char*)(BOOT_ROM_BASE + SIZE_KB(128) + SIZE_KB(48));
    const char* dmg = NULL;

    if (String_EqualsUpTo(p0, "SeFS", 4)) {
        dmg = p0;
    }
    else if (String_EqualsUpTo(p1, "SeFS", 4)) {
        dmg = p1;
    }
    // XXX


    // Create a RAM disk and copy the ROM disk image into it. We assume for now
    // that the disk image is exactly 64k in size. Just format the RAM disk with
    // a SerenaFS instance if there's no ROM disk image.
    try(RamDisk_Create(512, 128, 128, &pRamDisk));

    if (dmg) {
        for (LogicalBlockAddress lba = 0; lba < 128; lba++) {
            try(DiskDriver_PutBlock(pRamDisk, &dmg[lba * 512], lba));
        }
    }
    else {
        const FilePermissions ownerPerms = kFilePermission_Read | kFilePermission_Write | kFilePermission_Execute;
        const FilePermissions otherPerms = kFilePermission_Read | kFilePermission_Execute;
        const FilePermissions dirPerms = FilePermissions_Make(ownerPerms, otherPerms, otherPerms);

        try(SerenaFS_FormatDrive((DiskDriverRef)pRamDisk, kUser_Root, dirPerms));
    }


    // Create a SerenaFS instance and mount it as the root filesystem on the RAM
    // disk
    try(SerenaFS_Create((SerenaFSRef*)&pFS));
    try(FilesystemManager_Create(pFS, (DiskDriverRef)pRamDisk, &gFilesystemManager));
    return;

catch:
    print("Root filesystem mount failure: %d.\nHalting.\n", err);
    while(true);
    /* NOT REACHED */
}

// Called by the boot virtual processor after it has finished initializing all
// dispatch queue related services.
//
// This is the kernel main entry point which is responsible for bringing up the
// driver manager and the first process.
static void OnMain(void)
{
    // Create the driver manager
    try_bang(DriverManager_Create(&gDriverManager));


    // Initialize enough of the driver infrastructure so that we can start
    // printing to the console
    try_bang(DriverManager_AutoConfigureForConsole(gDriverManager));


    // Initialize the kernel print services
    print_init();


    // Debug printing
    //PrintClasses();


    // Initialize all other drivers
    try_bang(DriverManager_AutoConfigure(gDriverManager));


    // Initialize the Kernel Runtime Services so that we can make it available
    // to userspace in the form of the Userspace Runtime Services.
    krt_init();
    

    // Find and mount a root filesystem.
    init_root_filesystem();


    // Create the root process
    ProcessRef pRootProc;
    try_bang(RootProcess_Create(&pRootProc));


    // Create the process manager
    try_bang(ProcessManager_Create(pRootProc, &gProcessManager));


    // Get the root process going
    try_bang(RootProcess_Exec(pRootProc, (void*)0xfe0000));
}
