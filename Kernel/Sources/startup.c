//
//  startup.c
//  Apollo
//
//  Created by Dietmar Planitzer on 2/4/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "Foundation.h"
#include "Allocator.h"
#include "Bytes.h"
#include "Console.h"
#include "DispatchQueue.h"
#include "DriverManager.h"
#include "InterruptController.h"
#include "MonotonicClock.h"
#include "Platform.h"
#include "Process.h"
#include "VirtualProcessorScheduler.h"
#include "VirtualProcessorPool.h"

extern char _text, _etext, _data, _edata, _bss, _ebss;
static Byte* gKernelHeapBottom;
static Byte* gKernelHeapTop;


static _Noreturn OnStartup(const SystemDescription* _Nonnull pSysDesc);
static void OnMain(void);


// Find a suitable memory region for the boot kernel stack. We try to allocate
// the stack region from fast RAM and we try to put it as far up in RAM as
// possible. We only allocate from chip RAM if there is no fast RAM.
static Byte* _Nullable boot_allocate_kernel_stack(SystemDescription* _Nonnull pSysDesc, Int stackSize)
{
    Byte* pStackBase = NULL;
    
    for (Int i = pSysDesc->memory.descriptor_count - 1; i >= 0; i--) {
        const Int nAvailBytes = pSysDesc->memory.descriptor[i].upper - pSysDesc->memory.descriptor[i].lower;
        
        if (nAvailBytes >= stackSize) {
            pStackBase = pSysDesc->memory.descriptor[i].upper - stackSize;
            break;
        }
    }

    return pStackBase;
}


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
    Bytes_CopyRange((Byte*)&_data, (Byte*)&_etext, data_size);

    // Initialize the BSS segment
    Bytes_ClearRange((Byte*)&_bss, bss_size);


    // Carve the kernel data and bss out from memory descriptor #0 to ensure that
    // our kernel heap is not going to try to override the data/bss region.
    gKernelHeapBottom = pSysDesc->memory.descriptor[0].lower + Int_RoundUpToPowerOf2(data_size + bss_size, CPU_PAGE_SIZE);
    pSysDesc->memory.descriptor[0].lower = gKernelHeapBottom;


    // Store a reference to the system description in our globals
    gSystemDescription = pSysDesc;


    // Allocate the kernel stack in high mem
    const Int kernelStackSize = VP_DEFAULT_KERNEL_STACK_SIZE;
    Byte* pKernelStackBase = boot_allocate_kernel_stack(pSysDesc, kernelStackSize);
    assert(pKernelStackBase != NULL);
    gKernelHeapTop = pKernelStackBase;


    // Initialize the scheduler
    VirtualProcessorScheduler_CreateForLocalCPU(pSysDesc, VirtualProcessorClosure_MakeWithPreallocatedKernelStack((Closure1Arg_Func)OnStartup, (Byte*)pSysDesc, pKernelStackBase, kernelStackSize));


    // Do the first ever context switch over to the boot virtual processor
    // execution context.
    VirtualProcessorScheduler_IncipientContextSwitch();
}

static ErrorCode create_allocator(MemoryLayout* _Nonnull pMemLayout, AllocatorRef _Nullable * _Nonnull pOutAllocator)
{
    AllocatorRef pAllocator = NULL;
    decl_try_err();

    try(Allocator_Create(&pMemLayout->descriptor[0], &pAllocator));
    for (Int i = 1; i < pMemLayout->descriptor_count; i++) {
        try(Allocator_AddMemoryRegion(pAllocator, &pMemLayout->descriptor[i]));
    }

    *pOutAllocator = pAllocator;
    return EOK;

catch:
    *pOutAllocator = NULL;
    return err;
}

// Invoked by onBoot(). The code here runs in the boot virtual processor execution
// context. Interrupts and DMAs are still turned off.
//
// Phase 1 initialization is responsible for bringing up the interrupt handling,
// basic memort management, monotonic clock and the kernel main dispatch queue.
static _Noreturn OnStartup(const SystemDescription* _Nonnull pSysDesc)
{
    // Initialize the global heap
    //try_bang(Allocator_Create(&pSysDesc->memory, &gMainAllocator));
    try_bang(create_allocator(&pSysDesc->memory, &gMainAllocator));
    

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
    try_bang(DispatchQueue_Create(1, DISPATCH_QOS_INTERACTIVE, 0, gVirtualProcessorPool, NULL, (DispatchQueueRef*)&gMainDispatchQueue));
    
    
    // Enable interrupts
    cpu_enable_irqs();

    
    // Continue the kernel startup on the kernel main queue
    DispatchQueue_DispatchAsync(gMainDispatchQueue, DispatchQueueClosure_Make((Closure1Arg_Func)OnMain, NULL));

    
    // The boot virtual processor now takes over the duties of running the
    // virtual processor scheduler service tasks.
    VirtualProcessorScheduler_Run(gVirtualProcessorScheduler);
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


    // Initialize all other drivers
    try_bang(DriverManager_AutoConfigure(gDriverManager));

#if 0
    //print("_text: %p, _etext: %p, _data: %p, _edata: %p, _bss: %p, _ebss: %p\n\n", &_text, &_etext, &_data, &_edata, &_bss, &_ebss);
    // Create the root process and kick it off running
    try_bang(Process_Create(Process_GetNextAvailablePID(), &gRootProcess));
    Process_DispatchAsyncUser(gRootProcess, (Closure1Arg_Func)0xfe0000);
#else
    // XXX Unit tests
    void DispatchQueue_RunTests(void);

    //InterruptController_Dump(gInterruptController);
    DispatchQueue_RunTests();
    // XXX
#endif
}
