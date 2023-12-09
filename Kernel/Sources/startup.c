//
//  startup.c
//  Apollo
//
//  Created by Dietmar Planitzer on 2/4/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include <klib/klib.h>
#include "BootAllocator.h"
#include "DispatchQueue.h"
#include "DriverManager.h"
#include "FilesystemManager.h"
#include "InterruptController.h"
#include "MonotonicClock.h"
#include "Platform.h"
#include "Process.h"
#include "ProcessManager.h"
#include "RamFS.h"
#include "VirtualProcessorScheduler.h"
#include "VirtualProcessorPool.h"

extern char _text, _etext, _data, _edata, _bss, _ebss;
static Byte* gInitialHeapBottom;
static Byte* gInitialHeapTop;


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
    const Int data_size = &_edata - &_data;
    const Int bss_size = &_ebss - &_bss;

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
    VirtualProcessorScheduler_CreateForLocalCPU(pSysDesc, &boot_alloc, (Closure1Arg_Func)OnStartup, (Byte*)pSysDesc);


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
    try_bang(DispatchQueue_Create(0, 1, DISPATCH_QOS_INTERACTIVE, 0, gVirtualProcessorPool, NULL, (DispatchQueueRef*)&gMainDispatchQueue));
    
    
    // Enable interrupts
    cpu_enable_irqs();

    
    // Continue the kernel startup on the kernel main queue
    DispatchQueue_DispatchAsync(gMainDispatchQueue, DispatchQueueClosure_Make((Closure1Arg_Func)OnMain, NULL));

    
    // The boot virtual processor now takes over the duties of running the
    // virtual processor scheduler service tasks.
    VirtualProcessorScheduler_Run(gVirtualProcessorScheduler);
}

// Figures out from which filesystem to boot and sets up the initial global
// filesystem structure.
static InodeRef _Nonnull filesystem_init(void)
{
    FilesystemRef pRamFS;

    // XXX for now always a RAM disk
    User rootDirUser = {kRootUserId, kRootGroupId};
    try_bang(RamFS_Create(rootDirUser, (RamFSRef*)&pRamFS));
    try_bang(FilesystemManager_Create(pRamFS, &gFilesystemManager));
    Object_Release(pRamFS);

    return FilesystemManager_CopyRootNode(gFilesystemManager);
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


    // Figure out what boot filesystem to use and initialize the filesystem manager with it
    InodeRef pRootDir = filesystem_init();


#if 1
    // Create the root process
    ProcessRef pRootProc;
    try_bang(RootProcess_Create(pRootDir, &pRootProc));


    // Create the process manager
    try_bang(ProcessManager_Create(pRootProc, &gProcessManager));


    // Get the root process going
    try_bang(RootProcess_Exec(pRootProc, (void*)0xfe0000));
#else
    // XXX Unit tests
    void DispatchQueue_RunTests(void);

    //InterruptController_Dump(gInterruptController);
    DispatchQueue_RunTests();
    // XXX
#endif
}
