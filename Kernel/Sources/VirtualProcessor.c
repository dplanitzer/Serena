//
//  VirtualProcessor.c
//  Apollo
//
//  Created by Dietmar Planitzer on 2/21/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "VirtualProcessor.h"
#include "VirtualProcessorScheduler.h"
#include "Bytes.h"
#include "Heap.h"
#include "Platform.h"
#include "SystemGlobals.h"

#if __LP64__
#define STACK_ALIGNMENT  16
#elif __LP32__
#define STACK_ALIGNMENT  4
#else
#error "don't know how to align stack pointers"
#endif



extern void IdleVirtualProcessor_Run(Byte* _Nullable pContext);



// Allocates an execution stack.
// \param pStack the stack object (filled in on return)
// \param size the stack size
// \return the status
ErrorCode ExecutionStack_Init(ExecutionStack* _Nonnull pStack, Int size)
{
    pStack->base = NULL;
    pStack->size = 0;
    return ExecutionStack_SetMaxSize(pStack, size);
}

// Sets the size of the execution stack to the given size. Does not attempt to preserve
// the content of the existing stack.
ErrorCode ExecutionStack_SetMaxSize(ExecutionStack* _Nullable pStack, Int size)
{
    const Int newSize = (size > 0) ? Int_RoundUpToPowerOf2(size, STACK_ALIGNMENT) : 0;
    
    if (pStack->size != newSize) {
        kfree(pStack->base);
        pStack->size = newSize;
        pStack->base = kalloc(pStack->size, 0);
        if (pStack->base == NULL) {
            pStack->size = 0;
            return ENOMEM;
        }
    }
    
    return EOK;
}

// Frees the given stack.
// \param pStack the stack
void ExecutionStack_Destroy(ExecutionStack* _Nullable pStack)
{
    if (pStack) {
        kfree(pStack->base);
        pStack->base = NULL;
        pStack->size = 0;
    }
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Scheduler
////////////////////////////////////////////////////////////////////////////////


// Initializes a scheduler virtual processor. This is the virtual processor which
// is used to grandfather in the initial thread of execution at boot time. It is the
// first VP that is created for a physical processor. It then takes over duties for
// the scheduler.
// \param pVP the boot virtual processor record
// \param pSysDesc the system description
// \param pClosure the closure that should be invoked by the virtual processor
// \param pContext the context that should be passed to the closure
void SchedulerVirtualProcessor_Init(VirtualProcessor*_Nonnull pVP, const SystemDescription* _Nonnull pSysDesc, VirtualProcessor_Closure _Nonnull pClosure, Byte* _Nullable pContext)
{
    // Find a suitable memory region for the boot kernel stack. We try to allocate
    // the stack region from fast RAM and we try to put it as far up in RAM as
    // possible. We only allocate from chip RAM if there is no fast RAM.
    const Int nStackSize = VP_DEFAULT_KERNEL_STACK_SIZE;
    Byte* pStackBase = NULL;
    
    for (Int i = pSysDesc->memory_descriptor_count - 1; i >= 0; i--) {
        const Int nAvailBytes = pSysDesc->memory_descriptor[i].upper - pSysDesc->memory_descriptor[i].lower;
        
        if (nAvailBytes >= nStackSize) {
            pStackBase = pSysDesc->memory_descriptor[i].upper - nStackSize;
            break;
        }
    }
    
    assert(pStackBase != NULL);

    Bytes_ClearRange((Byte*)pVP, sizeof(VirtualProcessor));
    VirtualProcessor_CommonInit(pVP, VP_PRIORITY_HIGHEST);
    pVP->kernel_stack.base = pStackBase;
    pVP->kernel_stack.size = nStackSize;
    VirtualProcessor_SetClosure(pVP, pClosure, pContext);
    pVP->save_area.sr |= 0x0700;    // IRQs should be disabled by default
    pVP->state = kVirtualProcessorState_Ready;
    pVP->suspension_count = 0;
}

// Has to be called from the scheduler virtual processor context as early as possible
// at kernel initialization time and right after the heap has been initialized.
void SchedulerVirtualProcessor_FinishBoot(VirtualProcessor*_Nonnull pVP)
{
    SystemGlobals* pGlobals = SystemGlobals_Get();
    
    // Mark the boot kernel + user stack areas as allocated.
    assert(Heap_AllocateBytesAt(pGlobals->heap, pVP->kernel_stack.base, pVP->kernel_stack.size) != NULL);
}

void SchedulerVirtualProcessor_Run(Byte* _Nullable pContext)
{
    VirtualProcessorScheduler* pScheduler = VirtualProcessorScheduler_GetShared();
    List dead_vps;

    while (true) {
        List_Init(&dead_vps);
        const Int sps = VirtualProcessorScheduler_DisablePreemption();

        // Continue to wait as long as there's nothing to finalize
        while (List_IsEmpty(&pScheduler->finalizer_queue)) {
            (void)VirtualProcessorScheduler_WaitOn(pScheduler, &pScheduler->scheduler_wait_queue,
                                             TimeInterval_Add(MonotonicClock_GetCurrentTime(), TimeInterval_MakeSeconds(1)), true);
        }
        
        // Got some work to do. Save off the needed data in local vars and then
        // reenable preemption before we go and do the actual work.
        dead_vps = pScheduler->finalizer_queue;
        List_Deinit(&pScheduler->finalizer_queue);
        
        VirtualProcessorScheduler_RestorePreemption(sps);
        
        
        // XXX
        // XXX Should boost the priority of VPs that have been sitting on the
        // XXX ready queue for some time. Goal is to prevent a VP from getting
        // XXX starved to death because it's a very low priority one and there's
        // XXX always some higher priority VP sneaking in. Now a low priority
        // XXX VP which blocks will receive a boost once it is unblocked but this
        // XXX doesn't help background VPs which block rarely or not at all
        // XXX because blocking isn't a natural part of the algorithm they are
        // XXX executing. Eg a VP that scales an image in the background.
        // XXX Boost could be +1 priority every 1/4 second
        // XXX
        
        // Finalize VPs which have exited
        VirtualProcessor* pCurVP = (VirtualProcessor*)dead_vps.first;
        while (pCurVP) {
            VirtualProcessor* pNextVP = (VirtualProcessor*)pCurVP->rewa_queue_entry.next;
            
            VirtualProcessor_Destroy(pCurVP);
            pCurVP = pNextVP;
        }
    }
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Idle
////////////////////////////////////////////////////////////////////////////////


// Creates an idle virtual processor. The scheduler schedules this VP if no other
// one is in state ready.
VirtualProcessor* _Nullable IdleVirtualProcessor_Create(const SystemDescription* _Nonnull pSysDesc)
{
    VirtualProcessor* pVP = NULL;
    
    pVP = (VirtualProcessor*)kalloc(sizeof(VirtualProcessor), HEAP_ALLOC_OPTION_CLEAR);
    FailNULL(pVP);
    
    VirtualProcessor_CommonInit(pVP, VP_PRIORITY_LOWEST);
    VirtualProcessor_SetMaxKernelStackSize(pVP, VP_DEFAULT_KERNEL_STACK_SIZE);
    VirtualProcessor_SetClosure(pVP, (VirtualProcessor_Closure)IdleVirtualProcessor_Run, NULL);
    
    return pVP;
    
failed:
    kfree((Byte*)pVP);
    return NULL;
}

// Puts the CPU to sleep until an interrupt occurs. The interrupt will give the
// scheduler a chance to run some other virtual processor if one is ready.
void IdleVirtualProcessor_Run(Byte* _Nullable pContext)
{
    while (true) {
        cpu_sleep();
    }
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: VirtualProcessor
////////////////////////////////////////////////////////////////////////////////


// Frees a virtual processor.
// \param pVP the virtual processor
void __func_VirtualProcessor_Destroy(VirtualProcessor* _Nullable pVP)
{
    ListNode_Deinit(&pVP->owner.queue_entry);
    ExecutionStack_Destroy(&pVP->kernel_stack);
    ExecutionStack_Destroy(&pVP->user_stack);
    kfree((Byte*)pVP);
}

static const VirtualProcessorVTable gVirtualProcessorVTable = {
    __func_VirtualProcessor_Destroy
};


// Relinquishes the virtual processor which means that it is finished executing
// code and that it should be moved back to the virtual processor pool. This
// function does not return to the caller. This function should only be invoked
// from the bottom-most frame on the virtual processor's kernel stack.
void VirtualProcesssor_Relinquish(void)
{
    VirtualProcessorPool_RelinquishVirtualProcessor(VirtualProcessorPool_GetShared(), VirtualProcessor_GetCurrent());
    /* NOT REACHED */
}

// Initializes a virtual processor. A virtual processor always starts execution
// in supervisor mode. The user stack size may be 0.
//
// \param pVP the boot virtual processor record
// \param priority the initial VP priority
void VirtualProcessor_CommonInit(VirtualProcessor*_Nonnull pVP, Int priority)
{
    ListNode_Init(&pVP->rewa_queue_entry);
    ExecutionStack_Init(&pVP->kernel_stack, 0);
    ExecutionStack_Init(&pVP->user_stack, 0);

    pVP->vtable = &gVirtualProcessorVTable;
    
    ListNode_Init(&pVP->owner.queue_entry);
    pVP->owner.self = pVP;
    
    ListNode_Init(&pVP->timeout.queue_entry);
    
    pVP->timeout.deadline = kQuantums_Infinity;
    pVP->timeout.owner = (void*)pVP;
    pVP->timeout.is_valid = false;
    pVP->waiting_on_wait_queue = NULL;
    pVP->wakeup_reason = WAKEUP_REASON_NONE;
    
    pVP->state = kVirtualProcessorState_Suspended;
    pVP->flags = 0;
    pVP->priority = (Int8)priority;
    pVP->suspension_count = 1;
    
    SystemGlobals* pGlobals = SystemGlobals_Get();
    pVP->vpid = AtomicInt_Add(&pGlobals->next_available_vpid, 1);

    pVP->dispatchQueue = NULL;
    pVP->dispatchQueueConcurrenyLaneIndex = -1;
}

// Creates a new virtual processor.
// \return the new virtual processor; NULL if creation has failed
VirtualProcessor* _Nullable VirtualProcessor_Create(void)
{
    VirtualProcessor* pVP = NULL;
    
    pVP = (VirtualProcessor*)kalloc(sizeof(VirtualProcessor), HEAP_ALLOC_OPTION_CLEAR);
    FailNULL(pVP);
    
    VirtualProcessor_CommonInit(pVP, VP_PRIORITY_NORMAL);
    
    return pVP;
    
failed:
    kfree((Byte*)pVP);
    return NULL;
}

void VirtualProcessor_Destroy(VirtualProcessor* _Nullable pVP)
{
    if (pVP) {
        pVP->vtable->destroy(pVP);
    }
}

// Sets the dispatch queue that has acquired the virtual processor and owns it
// until the virtual processor is relinquished back to the virtual processor
// pool.
void VirtualProcessor_SetDispatchQueue(VirtualProcessor*_Nonnull pVP, void* _Nullable pQueue, Int concurrenyLaneIndex)
{
    pVP->dispatchQueue = pQueue;
    pVP->dispatchQueueConcurrenyLaneIndex = concurrenyLaneIndex;
}

// Sets the max size of the kernel stack. Changing the stack size of a VP is only
// allowed while the VP is suspended. Note that you must call SetClosure() on the
// VP after changing its stack to correctly initialize the stack pointers. A VP
// must always have a kernel stack.
ErrorCode VirtualProcessor_SetMaxKernelStackSize(VirtualProcessor*_Nonnull pVP, Int size)
{
    if (size < CPU_PAGE_SIZE) {
        return EPARAM;
    }
    if (pVP->state != kVirtualProcessorState_Suspended) {
        return EBUSY;
    }
    
    return ExecutionStack_SetMaxSize(&pVP->kernel_stack, size);
}

// Sets the max size of the user stack. Changing the stack size of a VP is only
// allowed while the VP is suspended. Note that you must call SetClosure() on the
// VP after changing its stack to correctly initialize the stack pointers. You may
// remove the user stack altogether by passing 0.
ErrorCode VirtualProcessor_SetMaxUserStackSize(VirtualProcessor*_Nonnull pVP, Int size)
{
    if (size < 0) {
        return EPARAM;
    }
    if (pVP->state != kVirtualProcessorState_Suspended) {
        return EBUSY;
    }
    
    return ExecutionStack_SetMaxSize(&pVP->user_stack, size);
}

// Sets the closure which the virtual processor should run when it is resumed.
// This function may only be called while the VP is suspended.
//
// \param pVP the virtual processor
// \param pClosure the closure that should be invoked
// \param pContext the context that should be passed to the closure
void VirtualProcessor_SetClosure(VirtualProcessor*_Nonnull pVP, VirtualProcessor_Closure _Nonnull pClosure, Byte* _Nullable pContext)
{
    Bytes_ClearRange((Byte*)&pVP->save_area, sizeof(CpuContext));
    
    pVP->save_area.usp = (UInt32) ExecutionStack_GetInitialTop(&pVP->user_stack);
    pVP->save_area.a[7] = (UInt32) ExecutionStack_GetInitialTop(&pVP->kernel_stack);
    pVP->save_area.pc = (UInt32) pClosure;


    // Note that we do not set up an initial stack frame on the user stack because
    // user space calls have to be done via cpu_call_as_user() and this function
    // takes care of setting up a frame on the user stack that will eventually
    // lead the user space code back to kernel space.
    
     
    // Every VP has a kernel stack. Set it up
    assert(pVP->kernel_stack.base != NULL);
    assert(pVP->kernel_stack.size >= 16);

    // Initial kernel stack frame looks like this:
    // SP + 12: pContext
    // SP +  8: RTS address (VirtualProcesssor_Relinquish() entry point)
    // SP +  6: Stack frame format for RTE (0 -> 8 byte frame size)
    // SP +  2: PC
    // SP +  0: SR
    //
    // Note that the RTE frame is necessary because the VirtualProcessorScheduler_SwitchContext
    // function expects to find an RTE frame on the kernel stack on entry
    Byte* sp = (Byte*) pVP->save_area.a[7];
    sp -= 4; *((Byte**)sp) = pContext;
    sp -= 4; *((Byte**)sp) = (Byte*)VirtualProcesssor_Relinquish;
    sp -= 2; *((UInt16*)sp) = 0;    // RTE frame format
    sp -= 4; *((UInt32*)sp) = pVP->save_area.pc;
    sp -= 2; *((UInt16*)sp) = pVP->save_area.sr;
    pVP->save_area.a[7] = (UInt32)sp;
    pVP->save_area.sr |= 0x2000;
}

// Reconfigures the flow of execution in the given virtual processor such that the
// closure 'pClosure' will be invoked like a subroutine call in user space. The
// interrupted flow of execution will be resumed at the point of interruption
// when 'pClosure' returns. The async call for 'pClosure' is arranged such that:
// 1) if VP is running in user space: 'pClosure' is invoked right away (like a subroutine)
// 2) if the VP is running in kernel space: 'pClosure' is invoked once the currently
//    active system call has completed. 'pClosure' is invoked by the syscall RTE
//    instruction and 'pClosure' in turn will return to the code that did the original
//    system call invocation. The invocation of 'pClosure' is completely transparent
//    to the user space code that originally invoked the system call. It does not
//    know that 'pClosure' was executed as a side effect of doing the system call.
//
// \param isNoReturn true if the closure does not return. This allows this function
//        to reset the user stack before invoking 'pClosure' which has the advantage
//        that the 'pClosure' call is guaranteed to work and that it will not run into
//        the end of the user stack. ONLY use this option if 'pClosure' is guranateed
//        to not return since it destroys the user stack state.
ErrorCode VirtualProcessor_ScheduleAsyncUserClosureInvocation(VirtualProcessor*_Nonnull pVP, VirtualProcessor_Closure _Nonnull pClosure, Byte* _Nullable pContext, Bool isNoReturn)
{
    UInt auci_options = 0;
    
    if (SystemDescription_GetShared()->fpu_model != FPU_MODEL_NONE) {
        auci_options |= CPU_AUCI_SAVE_FP_STATE;
    }
    
    VirtualProcessor_Suspend(pVP);

    if (isNoReturn) {
        pVP->save_area.usp = (UInt32) ExecutionStack_GetInitialTop(&pVP->user_stack);
    }
    
    if ((pVP->save_area.sr & 0x2000) != 0) {
        // Kernel space:
        // let the currently active system call finish and intercept the RTE
        // back out from the system call
        UInt32* pReturnAddress = (UInt32*)(pVP->syscall_entry_ksp + 2);
        
        cpu_push_async_user_closure_invocation(auci_options, &pVP->save_area.usp, *pReturnAddress, pClosure, pContext);
        *pReturnAddress = (UInt32)cpu_async_user_closure_trampoline;
    } else {
        // User space:
        // redirect the VP to the new call
        cpu_push_async_user_closure_invocation(auci_options, &pVP->save_area.usp, pVP->save_area.pc, pClosure, pContext);
        pVP->save_area.pc = (UInt32)cpu_async_user_closure_trampoline;
    }

    VirtualProcessor_Resume(pVP, false);
    
    return EOK;
}

// Exits the calling virtual processor. If possible then the virtual processor is
// moved back to the reuse cache. Otherwise it is destroyed for good.
void VirtualProcessor_Exit(VirtualProcessor* _Nonnull pVP)
{
    VirtualProcessorPool_RelinquishVirtualProcessor(VirtualProcessorPool_GetShared(), pVP);
    /* NOT REACHED */
}

void VirtualProcessor_Dump(VirtualProcessor* _Nonnull pVP)
{
    for (Int i = 0; i < 7; i++) {
        print("d%d: %p    a%d: %p\n", i, pVP->save_area.d[i], i, pVP->save_area.a[i]);
    }
    print("d7: %p   ssp: %p\n", pVP->save_area.d[7], pVP->save_area.a[7]);
    print("                usp: %p\n", pVP->save_area.usp);
    print("                 pc: %p\n", pVP->save_area.pc);
    print("                 sr: %p\n", pVP->save_area.sr);
}
