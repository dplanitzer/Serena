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


// Initializes an execution stack struct. The execution stack is empty by default
// and you need to call ExecutionStack_SetMaxSize() to allocate the stack with
// the required size.
// \param pStack the stack object (filled in on return)
void ExecutionStack_Init(ExecutionStack* _Nonnull pStack)
{
    pStack->base = NULL;
    pStack->size = 0;
}

// Sets the size of the execution stack to the given size. Does not attempt to preserve
// the content of the existing stack.
ErrorCode ExecutionStack_SetMaxSize(ExecutionStack* _Nullable pStack, Int size)
{
    const Int newSize = (size > 0) ? Int_RoundUpToPowerOf2(size, STACK_ALIGNMENT) : 0;
    
    if (pStack->size != newSize) {
        kfree(pStack->base);
        pStack->size = newSize;
        kalloc(pStack->size, (Byte**) &pStack->base);
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
_Noreturn VirtualProcesssor_Relinquish(void)
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
    Bytes_ClearRange((Byte*)&pVP->save_area, sizeof(CpuContext));

    ListNode_Init(&pVP->rewa_queue_entry);
    ExecutionStack_Init(&pVP->kernel_stack);
    ExecutionStack_Init(&pVP->user_stack);

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
    
    FailErr(kalloc_cleared(sizeof(VirtualProcessor), (Byte**) &pVP));
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
    VP_ASSERT_ALIVE(pVP);
    pVP->dispatchQueue = pQueue;
    pVP->dispatchQueueConcurrenyLaneIndex = concurrenyLaneIndex;
}

// Sets the closure which the virtual processor should run when it is resumed.
// This function may only be called while the VP is suspended.
//
// \param pVP the virtual processor
// \param closure the closure description
ErrorCode VirtualProcessor_SetClosure(VirtualProcessor*_Nonnull pVP, VirtualProcessorClosure closure)
{
    VP_ASSERT_ALIVE(pVP);
    assert(pVP->state == kVirtualProcessorState_Suspended);
    assert(closure.kernelStackSize >= VP_MIN_KERNEL_STACK_SIZE);

    ErrorCode err;

    if (closure.kernelStackBase == NULL) {
        if ((err = ExecutionStack_SetMaxSize(&pVP->kernel_stack, closure.kernelStackSize)) != EOK) {
            return err;
        }
    } else {
        ExecutionStack_SetMaxSize(&pVP->kernel_stack, 0);
        pVP->kernel_stack.base = closure.kernelStackBase;
        pVP->kernel_stack.size = closure.kernelStackSize;
    }
    if ((err = ExecutionStack_SetMaxSize(&pVP->user_stack, closure.userStackSize)) != EOK) {
        return err;
    }
    
    pVP->save_area.usp = (UInt32) ExecutionStack_GetInitialTop(&pVP->user_stack);
    pVP->save_area.a[7] = (UInt32) ExecutionStack_GetInitialTop(&pVP->kernel_stack);
    pVP->save_area.pc = (UInt32) closure.func;


    // Note that we do not set up an initial stack frame on the user stack because
    // user space calls have to be done via cpu_call_as_user() and this function
    // takes care of setting up a frame on the user stack that will eventually
    // lead the user space code back to kernel space.
    //
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
    sp -= 4; *((Byte**)sp) = closure.context;
    sp -= 4; *((Byte**)sp) = (Byte*)VirtualProcesssor_Relinquish;
    sp -= 2; *((UInt16*)sp) = 0;    // RTE frame format
    sp -= 4; *((UInt32*)sp) = pVP->save_area.pc;
    sp -= 2; *((UInt16*)sp) = pVP->save_area.sr;
    pVP->save_area.a[7] = (UInt32)sp;
    pVP->save_area.sr |= 0x2000;

    return EOK;
}

// Aborts an on-going call-as-user invocation and causes the cpu_call_as_user()
// invocation to return. Note that aborting a call-as-user invocation leaves the
// virtual processor's userspace stack in an indeterminate state. Consequently
// a call-as-user invocation should only be aborted if you no longer care about
// the state of the userspace. Eg if the goal is to terminate a process that may
// be in the middle of executing userspace code.
// What exactly happens when userspace code execution is aborted depends on
// whether the userspace code is currently executing in userspace or a system
// call:
// 1) running in userspace: execution is immediately aborted and no attempt is
//                          made to unwind the userspace stack or free any
//                          userspace resources.
// 2) executing a system call: the system call is allowed to run to completion.
//                             An additional mechanism is needed if the system
//                             call should be effectively cancelled. However the
//                             system call stack frame is altered such that the
//                             return from the system call will abort the
//                             call-as-user invocation.
ErrorCode VirtualProcessor_AbortCallAsUser(VirtualProcessor*_Nonnull pVP)
{
    const Bool isCallerRunningOnVpToManipulate = (VirtualProcessor_GetCurrent() == pVP);

    if (!isCallerRunningOnVpToManipulate) {
        VirtualProcessor_Suspend(pVP);
    }

    if ((pVP->save_area.sr & 0x2000) != 0) {
        // Kernel space:
        // let the currently active system call finish and intercept the RTE
        // back out from the system call
        UInt32* pReturnAddress = (UInt32*)(pVP->syscall_entry_ksp + 2);
        
        *pReturnAddress = (UInt32)cpu_abort_call_as_user;
    } else {
        // User space:
        // redirect the VP to the new call
        if (isCallerRunningOnVpToManipulate) {
            // Can not change the PC while we are executing on the VP that we are
            // suposed to manipulate
            abort();
        }
        pVP->save_area.pc = (UInt32)cpu_abort_call_as_user;
    }

    if (!isCallerRunningOnVpToManipulate) {
        VirtualProcessor_Resume(pVP, false);
    }
    
    return EOK;
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

// Terminates the virtual processor that is executing the caller. Does not return
// to the caller. Note that the actual termination of the virtual processor is
// handled by the virtual processor scheduler.
_Noreturn VirtualProcessor_Terminate(VirtualProcessor* _Nonnull pVP)
{
    VP_ASSERT_ALIVE(pVP)
    pVP->flags |= VP_FLAG_TERMINATED;

    VirtualProcessorScheduler_TerminateVirtualProcessor(VirtualProcessorScheduler_GetShared(), pVP);
    // NOT REACHED
}

// Sleep for the given number of seconds.
ErrorCode VirtualProcessor_Sleep(TimeInterval delay)
{
    VirtualProcessorScheduler* pScheduler = VirtualProcessorScheduler_GetShared();
    const TimeInterval curTime = MonotonicClock_GetCurrentTime();
    const TimeInterval deadline = TimeInterval_Add(curTime, delay);

    
    // Use the DelayUntil() facility for short waits and context switching for medium and long waits
    if (MonotonicClock_DelayUntil(deadline)) {
        return EOK;
    }
    
    
    // This is a medium or long wait -> context switch away
    Int sps = VirtualProcessorScheduler_DisablePreemption();
    const Int err = VirtualProcessorScheduler_WaitOn(pScheduler, &pScheduler->sleep_queue, deadline);
    VirtualProcessorScheduler_RestorePreemption(sps);
    
    return (err == EINTR) ? EINTR : EOK;
}

// Returns the priority of the given VP.
Int VirtualProcessor_GetPriority(VirtualProcessor* _Nonnull pVP)
{
    VP_ASSERT_ALIVE(pVP);
    const Int sps = VirtualProcessorScheduler_DisablePreemption();
    const Int pri = pVP->priority;
    
    VirtualProcessorScheduler_RestorePreemption(sps);
    return pri;
}

// Changes the priority of a virtual processor. Does not immediately reschedule
// the VP if it is currently running. Instead the VP is allowed to finish its
// current quanta.
// XXX might want to change that in the future?
void VirtualProcessor_SetPriority(VirtualProcessor* _Nonnull pVP, Int priority)
{
    VP_ASSERT_ALIVE(pVP);
    VirtualProcessorScheduler* pScheduler = VirtualProcessorScheduler_GetShared();
    Int sps = VirtualProcessorScheduler_DisablePreemption();
    
    if (pVP->priority != priority) {
        switch (pVP->state) {
            case kVirtualProcessorState_Ready: {
                VirtualProcessorScheduler_RemoveVirtualProcessor_Locked(pScheduler, pVP);
                pVP->priority = priority;
                VirtualProcessorScheduler_AddVirtualProcessor_Locked(pScheduler, pVP, pVP->priority);
                break;
            }
                
            case kVirtualProcessorState_Waiting:
            case kVirtualProcessorState_Suspended:
                pVP->priority = priority;
                break;
                
            case kVirtualProcessorState_Running:
                pVP->priority = priority;
                pVP->effectivePriority = priority;
                pVP->quantum_allowance = QuantumAllowanceForPriority(pVP->effectivePriority);
                break;
        }
    }
    VirtualProcessorScheduler_RestorePreemption(sps);
}

// Returns true if the given virtual processor is currently suspended; false otherwise.
Bool VirtualProcessor_IsSuspended(VirtualProcessor* _Nonnull pVP)
{
    VP_ASSERT_ALIVE(pVP);
    Bool isSuspended;
    const Int sps = VirtualProcessorScheduler_DisablePreemption();
    
    isSuspended = pVP->state == kVirtualProcessorState_Suspended;
    VirtualProcessorScheduler_RestorePreemption(sps);
    return isSuspended;
}

// Suspends the calling virtual processor. This function supports nested calls.
ErrorCode VirtualProcessor_Suspend(VirtualProcessor* _Nonnull pVP)
{
    VP_ASSERT_ALIVE(pVP);
    VirtualProcessorScheduler* pScheduler = VirtualProcessorScheduler_GetShared();
    const Int sps = VirtualProcessorScheduler_DisablePreemption();
    
    if (pVP->suspension_count == INT8_MAX) {
        VirtualProcessorScheduler_RestorePreemption(sps);
        return EPARAM;
    }
    
    const Int8 oldState = pVP->state;
    pVP->suspension_count++;
    pVP->state = kVirtualProcessorState_Suspended;
    
    switch (oldState) {
        case kVirtualProcessorState_Ready:
            VirtualProcessorScheduler_RemoveVirtualProcessor_Locked(pScheduler, pVP);
            break;
            
        case kVirtualProcessorState_Running:
            VirtualProcessorScheduler_SwitchTo(pScheduler,
                                               VirtualProcessorScheduler_GetHighestPriorityReady(pScheduler));
            break;
            
        case kVirtualProcessorState_Waiting:
            VirtualProcessorScheduler_WakeUpOne(pScheduler, pVP->waiting_on_wait_queue, pVP, WAKEUP_REASON_INTERRUPTED, false);
            break;
            
        case kVirtualProcessorState_Suspended:
            // We are already suspended
            break;
            
        default:
            abort();
            
    }
    
    VirtualProcessorScheduler_RestorePreemption(sps);
    return EOK;
}

// Resumes the given virtual processor. The virtual processor is forcefully
// resumed if 'force' is true. This means that it is resumed even if the suspension
// count is > 1.
ErrorCode VirtualProcessor_Resume(VirtualProcessor* _Nonnull pVP, Bool force)
{
    VP_ASSERT_ALIVE(pVP);
    VirtualProcessorScheduler* pScheduler = VirtualProcessorScheduler_GetShared();
    const Int sps = VirtualProcessorScheduler_DisablePreemption();
    
    if (pVP->suspension_count == 0) {
        VirtualProcessorScheduler_RestorePreemption(sps);
        return EPARAM;
    }
    
    pVP->suspension_count = force ? 0 : pVP->suspension_count - 1;
    
    if (pVP->suspension_count == 0) {
        VirtualProcessorScheduler_AddVirtualProcessor_Locked(pScheduler, pVP, pVP->priority);
        VirtualProcessorScheduler_MaybeSwitchTo(pScheduler, pVP);
    }
    
    VirtualProcessorScheduler_RestorePreemption(sps);
    return EOK;
}
