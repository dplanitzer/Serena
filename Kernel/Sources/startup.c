//
//  startup.c
//  Apollo
//
//  Created by Dietmar Planitzer on 2/4/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "Foundation.h"
#include "Bytes.h"
#include "Console.h"
#include "DispatchQueue.h"
#include "EventDriver.h"
#include "FloppyDisk.h"
#include "Heap.h"
#include "InterruptController.h"
#include "MonotonicClock.h"
#include "Platform.h"
#include "Process.h"
#include "RealtimeClock.h"
#include "VirtualProcessorScheduler.h"
#include "VirtualProcessorPool.h"

extern char _text, _etext, _data, _edata, _bss, _ebss;

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

    // Copy the kernel data segment from ROM to RAM
    Bytes_CopyRange((Byte*)&_data, (Byte*)&_etext, &_edata - &_data);

    // Initialize the BSS segment
    Bytes_ClearRange((Byte*)&_bss, &_ebss - &_bss);
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
    // Allocate the kernel stack in high mem
    const Int kernelStackSize = VP_DEFAULT_KERNEL_STACK_SIZE;
    Byte* pKernelStackBase = boot_allocate_kernel_stack(pSysDesc, kernelStackSize);
    assert(pKernelStackBase != NULL);


    // Initialize the boot virtual processor
    VirtualProcessor* pVp = NULL;
    try_bang(BootVirtualProcessor_CreateForLocalCPU(pSysDesc, VirtualProcessorClosure_MakeWithPreallocatedKernelStack((Closure1Arg_Func)OnStartup_Phase1, (Byte*)pSysDesc, pKernelStackBase, kernelStackSize), &pVp));

    
    // Initialize the scheduler
    VirtualProcessorScheduler_CreateForLocalCPU(pSysDesc, pVp);
}

// Called by the boot virtual processor after onBoot() has returned. This is the thread
// of execution that was started by the Reset() function.
//
// Phase 1 initialization is responsible for bringing up the interrupt handling,
// basic memort management, monotonic clock and the kernel main dispatch queue.
static _Noreturn OnStartup_Phase1(const SystemDescription* _Nonnull pSysDesc)
{
    // Initialize the global heap
    try_bang(Heap_Create(pSysDesc->memory_descriptor, pSysDesc->memory_descriptor_count, &gHeap));
    

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
    
    
    // Enable interrupt handling
    cpu_enable_irqs();

    
    // Continue the kernel startup on the kernel main queue
    DispatchQueue_DispatchAsync(gMainDispatchQueue, DispatchQueueClosure_Make((Closure1Arg_Func)OnStartup_Phase2, (Byte*)pSysDesc));

    
    // The boot virtual processor now takes over the duties of running the
    // virtual processor scheduler service tasks.
    VirtualProcessorScheduler_Run(gVirtualProcessorScheduler);
}

// Called by the boot virtual processor after it has finished initializing all
// dispatch queue related services.
//
// Phase 2 initialization is responsible for bringing up the device manager and
// the first process.
static void OnStartup_Phase2(const SystemDescription* _Nonnull pSysDesc)
{
    // Initialize print()
    print_init();

    
    // Initialize I/O services needed by the console
    try_bang(RealtimeClock_Create(pSysDesc, &gRealtimeClock));
    
    const VideoConfiguration* pVideoConfig;
    if (chipset_is_ntsc()) {
        pVideoConfig = &kVideoConfig_NTSC_640_200_60;
        //pVideoConfig = &kVideoConfig_NTSC_640_400_30;
    } else {
        pVideoConfig = &kVideoConfig_PAL_640_256_50;
        //pVideoConfig = &kVideoConfig_PAL_640_512_25;
    }
    
    try_bang(GraphicsDriver_Create(pVideoConfig, kPixelFormat_RGB_Indexed1, &gMainGDevice));
    
    
    // Initialize the console
    try_bang(Console_Create(gMainGDevice, &gConsole));

    
    // Initialize all other I/O dervices
    try_bang(FloppyDMA_Create(&gFloppyDma));
    try_bang(EventDriver_Create(gMainGDevice, &gEventDriver));
    

#if 1
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
