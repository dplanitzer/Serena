//
//  VirtualProcessor.c
//  Apollo
//
//  Created by Dietmar Planitzer on 2/21/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "VirtualProcessor.h"
#include "kalloc.h"
#include "VirtualProcessorPool.h"
#include "VirtualProcessorScheduler.h"
#include "Bytes.h"
#include "Platform.h"


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
    decl_try_err();
    const Int newSize = (size > 0) ? Int_RoundUpToPowerOf2(size, STACK_ALIGNMENT) : 0;
    
    if (pStack->size != newSize) {
        kfree(pStack->base);
        pStack->size = newSize;
        // XXX the allocation may fail which means that we should keep the old stack around
        // XXX so that we can ensure that the VP doesn't suddenly stand there with its pants
        // XXX way down. However we don't worry about this right now since we'll move to virtual
        // XXX memory anyway.
        try(kalloc(pStack->size, (Byte**) &pStack->base));
    }
    
    return EOK;

catch:
    pStack->base = NULL;
    pStack->size = 0;
    return err;
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
    VirtualProcessorPool_RelinquishVirtualProcessor(gVirtualProcessorPool, VirtualProcessor_GetCurrent());
    /* NOT REACHED */
}

// Initializes a virtual processor. A virtual processor always starts execution
// in supervisor mode. The user stack size may be 0. Note that a virtual processor
// always starts out in suspended state.
//
// \param pVP the boot virtual processor record
// \param priority the initial VP priority
void VirtualProcessor_CommonInit(VirtualProcessor*_Nonnull pVP, Int priority)
{
    static volatile AtomicInt gNextAvailableVpid = 0;

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
    
    pVP->state = kVirtualProcessorState_Ready;
    pVP->flags = 0;
    pVP->priority = (Int8)priority;
    pVP->suspension_count = 1;
    
    pVP->vpid = AtomicInt_Add(&gNextAvailableVpid, 1);

    pVP->dispatchQueue = NULL;
    pVP->dispatchQueueConcurrenyLaneIndex = -1;
}

// Creates a new virtual processor.
// \return the new virtual processor; NULL if creation has failed
ErrorCode VirtualProcessor_Create(VirtualProcessor* _Nullable * _Nonnull pOutVP)
{
    decl_try_err();
    VirtualProcessor* pVP = NULL;
    
    try(kalloc_cleared(sizeof(VirtualProcessor), (Byte**) &pVP));
    VirtualProcessor_CommonInit(pVP, VP_PRIORITY_NORMAL);
    
    *pOutVP = pVP;
    return EOK;
    
catch:
    kfree((Byte*)pVP);
    *pOutVP = NULL;
    return err;
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
    assert(pVP->suspension_count > 0);
    assert(closure.kernelStackSize >= VP_MIN_KERNEL_STACK_SIZE);

    decl_try_err();

    if (closure.kernelStackBase == NULL) {
        try(ExecutionStack_SetMaxSize(&pVP->kernel_stack, closure.kernelStackSize));
    } else {
        ExecutionStack_SetMaxSize(&pVP->kernel_stack, 0);
        pVP->kernel_stack.base = closure.kernelStackBase;
        pVP->kernel_stack.size = closure.kernelStackSize;
    }
    try(ExecutionStack_SetMaxSize(&pVP->user_stack, closure.userStackSize));
    
    Bytes_ClearRange((Byte*)&pVP->save_area, sizeof(CpuContext));
    pVP->save_area.a[7] = (UInt32) ExecutionStack_GetInitialTop(&pVP->kernel_stack);
    pVP->save_area.usp = (UInt32) ExecutionStack_GetInitialTop(&pVP->user_stack);
    pVP->save_area.pc = (UInt32) closure.func;
    pVP->save_area.sr = 0x2000;     // We start out in supervisor mode


    // User stack:
    //
    // Note that we do not set up an initial stack frame on the user stack because
    // user space calls have to be done via cpu_call_as_user() and this function
    // takes care of setting up a frame on the user stack that will eventually
    // lead the user space code back to kernel space.
    //
    //
    // Kernel stack:
    //
    // The initial kernel stack frame looks like this:
    // SP + 12: pContext
    // SP +  8: RTS address (VirtualProcesssor_Relinquish() entry point)
    // SP +  0: dummy format $0 exception stack frame (8 byte size)
    //
    // See __rtecall_VirtualProcessorScheduler_SwitchContext for an explanation
    // of why we need the dummy exception stack frame.
    Byte* sp = (Byte*) pVP->save_area.a[7];
    sp -= 4; *((Byte**)sp) = closure.context;
    sp -= 4; *((Byte**)sp) = (Byte*)VirtualProcesssor_Relinquish;
    sp -= 4; *((UInt32*)sp) = 0;
    sp -= 4; *((UInt32*)sp) = 0;
    pVP->save_area.a[7] = (UInt32)sp;

    return EOK;

catch:
    return err;
}

// Invokes the given closure in user space. Preserves the kernel integer register
// state. Note however that this function does not preserve the floating point 
// register state. Call-as-user invocations can not be nested.
void VirtualProcessor_CallAsUser(VirtualProcessor* _Nonnull pVP, Closure1Arg_Func _Nonnull pClosure, Byte* _Nullable pContext)
{
    assert((pVP->flags & VP_FLAG_CAU_IN_PROGRESS) == 0);

    pVP->flags |= VP_FLAG_CAU_IN_PROGRESS;
    cpu_call_as_user((Cpu_UserClosure) pClosure, pContext);
    pVP->flags &= ~(VP_FLAG_CAU_IN_PROGRESS|VP_FLAG_CAU_ABORTED);
}

// Aborts an on-going call-as-user invocation and causes the
// VirtualProcessor_CallAsUser() call to return. Does nothing if the VP is not
// currently executing a call-as-user invocation.
// Note that aborting a call-as-user invocation leaves the virtual processor's
// userspace stack in an indeterminate state. Consequently a call-as-user
// invocation should only be aborted if you no longer care about the state of
// the userspace. Eg if the goal is to terminate a process that may be in the
// middle of executing userspace code.
// What exactly happens when userspace code execution is aborted depends on
// whether the userspace code is currently executing in userspace or a system
// call:
// 1) running in userspace: execution is immediately aborted and no attempt is
//                          made to unwind the userspace stack or free any
//                          userspace resources.
// 2) executing a system call: the system call is allowed to run to completion.
//                          However all interruptable waits will be interrupted
//                          no matter whether the VP is currently sitting in an
//                          interruptable wait or it enters it. This behavior
//                          will stay in effect until the system call has
//                          completed. Once the system call has finished and the
//                          call-as-user invocation has been aborted, waits will
//                          not be interrupted anymore.
ErrorCode VirtualProcessor_AbortCallAsUser(VirtualProcessor*_Nonnull pVP)
{
    decl_try_err();
    const Bool isCallerRunningOnVpToManipulate = (VirtualProcessor_GetCurrent() == pVP);

    if (!isCallerRunningOnVpToManipulate) {
        try(VirtualProcessor_Suspend(pVP));
    }

    if ((pVP->flags & VP_FLAG_CAU_IN_PROGRESS) != 0) {
        pVP->flags |= VP_FLAG_CAU_ABORTED;

        if ((pVP->save_area.sr & 0x2000) != 0) {
            // Kernel space:
            // let the currently active system call finish and redirect the RTE
            // from the system call back to user space to point to the call-as-user
            // abort function.
            // Why are we changing the return address of the RTE instead of letting
            // the system call check the state of VP_FLAG_CAU_ABORTED right before
            // it returns?
            // Because checking the flag would be unreliable. The problem is that
            // we might suspend the VP right after it has checked the flag and
            // before it is executing the RTE. So the system call would miss the
            // abort. Changing the RTE return address avoids this problem and
            // ensures that the system call will never miss an abort.
            UInt32* pReturnAddress = (UInt32*)(pVP->syscall_entry_ksp + 2);
            *pReturnAddress = (UInt32)cpu_abort_call_as_user;


            // The system call may currently be waiting on something (some
            // resource). Interrupt the wait. If the system call tries to do
            // aditional waits on its way back out to user space, then all those
            // (interruptable) waits will be immediately aborted since the call-
            // as-user invocation is now marked as aborted.
            if (pVP->state == kVirtualProcessorState_Waiting) {
                VirtualProcessorScheduler_WakeUpSome(gVirtualProcessorScheduler,
                                                 pVP->waiting_on_wait_queue,
                                                 INT_MAX,
                                                 WAKEUP_REASON_INTERRUPTED,
                                                 false);
            }
        } else {
            // User space:
            // redirect the VP to the new call
            pVP->save_area.pc = (UInt32)cpu_abort_call_as_user;
        }

        if (!isCallerRunningOnVpToManipulate) {
            VirtualProcessor_Resume(pVP, false);
        }
    }

    return EOK;

catch:
    return err;
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
    VP_ASSERT_ALIVE(pVP);
    pVP->flags |= VP_FLAG_TERMINATED;

    VirtualProcessorScheduler_TerminateVirtualProcessor(gVirtualProcessorScheduler, pVP);
    // NOT REACHED
}

// Sleep for the given number of seconds.
ErrorCode VirtualProcessor_Sleep(TimeInterval delay)
{
    const TimeInterval curTime = MonotonicClock_GetCurrentTime();
    const TimeInterval deadline = TimeInterval_Add(curTime, delay);

    
    // Use the DelayUntil() facility for short waits and context switching for medium and long waits
    if (MonotonicClock_DelayUntil(deadline)) {
        return EOK;
    }
    
    
    // This is a medium or long wait -> context switch away
    Int sps = VirtualProcessorScheduler_DisablePreemption();
    const Int err = VirtualProcessorScheduler_WaitOn(
                            gVirtualProcessorScheduler,
                            &gVirtualProcessorScheduler->sleep_queue, 
                            deadline,
                            true);
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
    const Int sps = VirtualProcessorScheduler_DisablePreemption();
    
    if (pVP->priority != priority) {
        switch (pVP->state) {
            case kVirtualProcessorState_Ready:
                if (pVP->suspension_count == 0) {
                    VirtualProcessorScheduler_RemoveVirtualProcessor_Locked(gVirtualProcessorScheduler, pVP);
                }
                pVP->priority = priority;
                if (pVP->suspension_count == 0) {
                    VirtualProcessorScheduler_AddVirtualProcessor_Locked(gVirtualProcessorScheduler, pVP, pVP->priority);
                }
                break;
                
            case kVirtualProcessorState_Waiting:
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
    const Int sps = VirtualProcessorScheduler_DisablePreemption();
    const Bool isSuspended = pVP->suspension_count > 0;
    VirtualProcessorScheduler_RestorePreemption(sps);
    return isSuspended;
}

// Suspends the calling virtual processor. This function supports nested calls.
ErrorCode VirtualProcessor_Suspend(VirtualProcessor* _Nonnull pVP)
{
    VP_ASSERT_ALIVE(pVP);
    const Int sps = VirtualProcessorScheduler_DisablePreemption();
    
    if (pVP->suspension_count == INT8_MAX) {
        VirtualProcessorScheduler_RestorePreemption(sps);
        return EPARAM;
    }
    
    pVP->suspension_count++;
    
    switch (pVP->state) {
        case kVirtualProcessorState_Ready:
            VirtualProcessorScheduler_RemoveVirtualProcessor_Locked(gVirtualProcessorScheduler, pVP);
            break;
            
        case kVirtualProcessorState_Running:
            // We're running, thus we are not on the ready queue. Do a forced
            // context switch to some other VP.
            VirtualProcessorScheduler_SwitchTo(gVirtualProcessorScheduler,
                                               VirtualProcessorScheduler_GetHighestPriorityReady(gVirtualProcessorScheduler));
            break;
            
        case kVirtualProcessorState_Waiting:
            // We do not interrupt the wait. It's just a longer wait
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
void VirtualProcessor_Resume(VirtualProcessor* _Nonnull pVP, Bool force)
{
    VP_ASSERT_ALIVE(pVP);
    const Int sps = VirtualProcessorScheduler_DisablePreemption();
    
    if (pVP->suspension_count > 0) {
        pVP->suspension_count = force ? 0 : pVP->suspension_count - 1;

        if (pVP->suspension_count == 0) {
            switch (pVP->state) {
                case kVirtualProcessorState_Ready:
                    VirtualProcessorScheduler_AddVirtualProcessor_Locked(gVirtualProcessorScheduler, pVP, pVP->priority);
                    break;
            
                case kVirtualProcessorState_Running:
                    VirtualProcessorScheduler_AddVirtualProcessor_Locked(gVirtualProcessorScheduler, pVP, pVP->priority);
                    VirtualProcessorScheduler_MaybeSwitchTo(gVirtualProcessorScheduler, pVP);
                    break;
            
                case kVirtualProcessorState_Waiting:
                    // Still in waiting state -> nothing more to do
                    break;
            
                default:
                    abort();            
            }
        }
    }
    VirtualProcessorScheduler_RestorePreemption(sps);
}
