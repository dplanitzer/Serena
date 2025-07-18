//
//  VirtualProcessor.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/21/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "VirtualProcessor.h"
#include "VirtualProcessorPool.h"
#include "VirtualProcessorScheduler.h"
#include "WaitQueue.h"
#include <hal/MonotonicClock.h>
#include <hal/Platform.h>
#include <kern/kalloc.h>
#include <kern/limits.h>
#include <kern/string.h>
#include <kern/timespec.h>
#include <kpi/signal.h>
#include <log/Log.h>


const sigset_t SIGSET_BLOCK_ALL = UINT32_MAX;
const sigset_t SIGSET_BLOCK_NONE = 0;


// Initializes an execution stack struct. The execution stack is empty by default
// and you need to call ExecutionStack_SetMaxSize() to allocate the stack with
// the required size.
// \param pStack the stack object (filled in on return)
void ExecutionStack_Init(ExecutionStack* _Nonnull self)
{
    self->base = NULL;
    self->size = 0;
}

// Sets the size of the execution stack to the given size. Does not attempt to preserve
// the content of the existing stack.
errno_t ExecutionStack_SetMaxSize(ExecutionStack* _Nullable self, size_t size)
{
    decl_try_err();
    const size_t newSize = (size > 0) ? __Ceil_PowerOf2(size, STACK_ALIGNMENT) : 0;
    
    if (self->size != newSize) {
        void* nsp = NULL;

        if (newSize > 0) {
            err = kalloc(newSize, (void**) &nsp);
            if (err != EOK) {
                return err;
            }
        }

        kfree(self->base);
        self->base = nsp;
        self->size = newSize;
    }
    
    return EOK;
}

// Frees the given stack.
// \param pStack the stack
void ExecutionStack_Destroy(ExecutionStack* _Nullable self)
{
    if (self) {
        kfree(self->base);
        self->base = NULL;
        self->size = 0;
    }
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: VirtualProcessor
////////////////////////////////////////////////////////////////////////////////


// Frees a virtual processor.
// \param pVP the virtual processor
void __func_VirtualProcessor_Destroy(VirtualProcessor* _Nullable self)
{
    ListNode_Deinit(&self->owner_qe);
    ExecutionStack_Destroy(&self->kernel_stack);
    ExecutionStack_Destroy(&self->user_stack);
    kfree(self);
}

static const VirtualProcessorVTable gVirtualProcessorVTable = {
    __func_VirtualProcessor_Destroy
};


// Relinquishes the virtual processor which means that it is finished executing
// code and that it should be moved back to the virtual processor pool. This
// function does not return to the caller. This function should only be invoked
// from the bottom-most frame on the virtual processor's kernel stack.
_Noreturn VirtualProcessor_Relinquish(void)
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
void VirtualProcessor_CommonInit(VirtualProcessor*_Nonnull self, int priority)
{
    static volatile AtomicInt gNextAvailableVpid = 0;

    ListNode_Init(&self->rewa_qe);
    ExecutionStack_Init(&self->kernel_stack);
    ExecutionStack_Init(&self->user_stack);

    self->vtable = &gVirtualProcessorVTable;
    
    ListNode_Init(&self->owner_qe);    
    ListNode_Init(&self->timeout.queue_entry);
    
    self->psigs = 0;
    self->sigmask = 0;

    self->timeout.deadline = kQuantums_Infinity;
    self->timeout.is_valid = false;
    self->waiting_on_wait_queue = NULL;
    self->wakeup_reason = 0;
    
    self->sched_state = SCHED_STATE_READY;
    self->flags = 0;
    self->priority = (int8_t)priority;
    self->suspension_count = 1;
    
    self->vpid = (vcpuid_t)AtomicInt_Add(&gNextAvailableVpid, 1);
    self->lifecycle_state = VP_LIFECYCLE_RELINQUISHED;

    self->dispatchQueue = NULL;
    self->dispatchQueueConcurrencyLaneIndex = -1;
}

// Creates a new virtual processor.
// \return the new virtual processor; NULL if creation has failed
errno_t VirtualProcessor_Create(VirtualProcessor* _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    VirtualProcessor* self = NULL;
    
    err = kalloc_cleared(sizeof(VirtualProcessor), (void**) &self);
    if (err == EOK) {
        VirtualProcessor_CommonInit(self, VP_PRIORITY_NORMAL);
    }

    *pOutSelf = self;
    return err;
}

void VirtualProcessor_Destroy(VirtualProcessor* _Nullable self)
{
    if (self) {
        self->vtable->destroy(self);
    }
}

// Sets the dispatch queue that has acquired the virtual processor and owns it
// until the virtual processor is relinquished back to the virtual processor
// pool.
void VirtualProcessor_SetDispatchQueue(VirtualProcessor*_Nonnull self, void* _Nullable pQueue, int concurrencyLaneIndex)
{
    VP_ASSERT_ALIVE(self);
    self->dispatchQueue = pQueue;
    self->dispatchQueueConcurrencyLaneIndex = concurrencyLaneIndex;
}

// Sets the closure which the virtual processor should run when it is resumed.
// This function may only be called while the VP is suspended.
//
// \param pVP the virtual processor
// \param closure the closure description
errno_t VirtualProcessor_SetClosure(VirtualProcessor*_Nonnull self, const VirtualProcessorClosure* _Nonnull closure)
{
    VP_ASSERT_ALIVE(self);
    assert(self->suspension_count > 0);
    assert(closure->kernelStackSize >= VP_MIN_KERNEL_STACK_SIZE);

    decl_try_err();

    if (closure->kernelStackBase == NULL) {
        try(ExecutionStack_SetMaxSize(&self->kernel_stack, closure->kernelStackSize));
    } else {
        ExecutionStack_SetMaxSize(&self->kernel_stack, 0);
        self->kernel_stack.base = closure->kernelStackBase;
        self->kernel_stack.size = closure->kernelStackSize;
    }
    try(ExecutionStack_SetMaxSize(&self->user_stack, closure->userStackSize));
    

    // Initialize the CPU context
    cpu_make_callout(&self->save_area, 
        (void*)ExecutionStack_GetInitialTop(&self->kernel_stack),
        (void*)ExecutionStack_GetInitialTop(&self->user_stack),
        false,
        closure->func,
        closure->context,
        (closure->ret_func) ? closure->ret_func : (VoidFunc_0)VirtualProcessor_Relinquish);

    return EOK;

catch:
    return err;
}

// Invokes the given closure in user space. Preserves the kernel integer register
// state. Note however that this function does not preserve the floating point 
// register state. Call-as-user invocations can not be nested.
void VirtualProcessor_CallAsUser(VirtualProcessor* _Nonnull self, VoidFunc_2 _Nonnull func, void* _Nullable context, void* _Nullable arg)
{
    assert((self->flags & VP_FLAG_CAU_IN_PROGRESS) == 0);

    self->flags |= VP_FLAG_CAU_IN_PROGRESS;
    cpu_call_as_user(func, context, arg);
    self->flags &= ~(VP_FLAG_CAU_IN_PROGRESS|VP_FLAG_CAU_ABORTED);
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
errno_t VirtualProcessor_AbortCallAsUser(VirtualProcessor*_Nonnull self)
{
    decl_try_err();
    const bool isCallerRunningOnVpToManipulate = (VirtualProcessor_GetCurrent() == self);

    if (!isCallerRunningOnVpToManipulate) {
        try(VirtualProcessor_Suspend(self));
    }

    if ((self->flags & VP_FLAG_CAU_IN_PROGRESS) != 0) {
        self->flags |= VP_FLAG_CAU_ABORTED;

        if ((self->save_area.sr & 0x2000) != 0) {
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
            uint32_t* pReturnAddress = (uint32_t*)(self->syscall_entry_ksp + 2);
            *pReturnAddress = (uint32_t)cpu_abort_call_as_user;


            // The system call may currently be waiting on something (some
            // resource). Interrupt the wait. If the system call tries to do
            // additional waits on its way back out to user space, then all those
            // (interruptable) waits will be immediately aborted since the call-
            // as-user invocation is now marked as aborted.
            if (self->sched_state == SCHED_STATE_WAITING) {
                WaitQueue_Wakeup(self->waiting_on_wait_queue,
                                    WAKEUP_ALL,
                                    WRES_WAKEUP);
            }
        } else {
            // User space:
            // redirect the VP to the new call
            self->save_area.pc = (uint32_t)cpu_abort_call_as_user;
        }

        if (!isCallerRunningOnVpToManipulate) {
            VirtualProcessor_Resume(self, false);
        }
    }

    return EOK;

catch:
    return err;
}

#if 0
void VirtualProcessor_Dump(VirtualProcessor* _Nonnull self)
{
    for (int i = 0; i < 7; i++) {
        printf("d%d: %p    a%d: %p\n", i, self->save_area.d[i], i, self->save_area.a[i]);
    }
    printf("d7: %p   ssp: %p\n", self->save_area.d[7], self->save_area.a[7]);
    printf("                usp: %p\n", self->save_area.usp);
    printf("                 pc: %p\n", self->save_area.pc);
    printf("                 sr: %p\n", self->save_area.sr);
}
#endif

// Terminates the virtual processor that is executing the caller. Does not return
// to the caller. Note that the actual termination of the virtual processor is
// handled by the virtual processor scheduler.
_Noreturn VirtualProcessor_Terminate(VirtualProcessor* _Nonnull self)
{
    VP_ASSERT_ALIVE(self);
    self->lifecycle_state = VP_LIFECYCLE_TERMINATING;

    // NOTE: We don't need to save the old preemption state because this VP is
    // going away and we will never context switch back to it. The context switch
    // will reenable preemption.
    (void) preempt_disable();
    VirtualProcessorScheduler_TerminateVirtualProcessor(gVirtualProcessorScheduler, self);
    /* NOT REACHED */
}

// Returns the priority of the given VP.
int VirtualProcessor_GetPriority(VirtualProcessor* _Nonnull self)
{
    VP_ASSERT_ALIVE(self);
    const int sps = preempt_disable();
    const int pri = self->priority;
    
    preempt_restore(sps);
    return pri;
}

// Changes the priority of a virtual processor. Does not immediately reschedule
// the VP if it is currently running. Instead the VP is allowed to finish its
// current quanta.
// XXX might want to change that in the future?
void VirtualProcessor_SetPriority(VirtualProcessor* _Nonnull self, int priority)
{
    VP_ASSERT_ALIVE(self);
    const int sps = preempt_disable();
    
    if (self->priority != priority) {
        switch (self->sched_state) {
            case SCHED_STATE_READY:
                if (self->suspension_count == 0) {
                    VirtualProcessorScheduler_RemoveVirtualProcessor_Locked(gVirtualProcessorScheduler, self);
                }
                self->priority = priority;
                if (self->suspension_count == 0) {
                    VirtualProcessorScheduler_AddVirtualProcessor_Locked(gVirtualProcessorScheduler, self, self->priority);
                }
                break;
                
            case SCHED_STATE_WAITING:
                self->priority = priority;
                break;
                
            case SCHED_STATE_RUNNING:
                self->priority = priority;
                self->effectivePriority = priority;
                self->quantum_allowance = QuantumAllowanceForPriority(self->effectivePriority);
                break;
        }
    }
    preempt_restore(sps);
}

// Yields the remainder of the current quantum to other VPs.
void VirtualProcessor_Yield(void)
{
    const int sps = preempt_disable();
    VirtualProcessor* self = (VirtualProcessor*)gVirtualProcessorScheduler->running;

    assert(self->sched_state == SCHED_STATE_RUNNING && self->suspension_count == 0);

    VirtualProcessorScheduler_AddVirtualProcessor_Locked(
        gVirtualProcessorScheduler, self, self->priority);
    VirtualProcessorScheduler_SwitchTo(gVirtualProcessorScheduler,
        VirtualProcessorScheduler_GetHighestPriorityReady(gVirtualProcessorScheduler));
    
    preempt_restore(sps);
}

// Suspends the calling virtual processor. This function supports nested calls.
errno_t VirtualProcessor_Suspend(VirtualProcessor* _Nonnull self)
{
    VP_ASSERT_ALIVE(self);
    const int sps = preempt_disable();
    
    if (self->suspension_count == INT8_MAX) {
        preempt_restore(sps);
        return EINVAL;
    }
    
    self->suspension_count++;
    
    if (self->suspension_count == 1) {
        self->suspension_time = MonotonicClock_GetCurrentQuantums();

        switch (self->sched_state) {
            case SCHED_STATE_READY:
                VirtualProcessorScheduler_RemoveVirtualProcessor_Locked(gVirtualProcessorScheduler, self);
                break;
            
            case SCHED_STATE_RUNNING:
                // We're running, thus we are not on the ready queue. Do a forced
                // context switch to some other VP.
                VirtualProcessorScheduler_SwitchTo(gVirtualProcessorScheduler,
                                                   VirtualProcessorScheduler_GetHighestPriorityReady(gVirtualProcessorScheduler));
                break;
            
            case SCHED_STATE_WAITING:
                WaitQueue_SuspendOne(self->waiting_on_wait_queue, self);
                break;
            
            default:
                abort();
        }
    }
    
    preempt_restore(sps);
    return EOK;
}

// Resumes the given virtual processor. The virtual processor is forcefully
// resumed if 'force' is true. This means that it is resumed even if the suspension
// count is > 1.
void VirtualProcessor_Resume(VirtualProcessor* _Nonnull self, bool force)
{
    VP_ASSERT_ALIVE(self);
    const int sps = preempt_disable();
    
    if (self->suspension_count == 0) {
        preempt_restore(sps);
        return;
    }


    if (force) {
        self->suspension_count = 0;
    }
    else {
        self->suspension_count--;
    }


    if (self->suspension_count == 0) {
        switch (self->sched_state) {
            case SCHED_STATE_READY:
            case SCHED_STATE_RUNNING:
                VirtualProcessorScheduler_AddVirtualProcessor_Locked(gVirtualProcessorScheduler, self, self->priority);
                VirtualProcessorScheduler_MaybeSwitchTo(gVirtualProcessorScheduler, self);
                break;
            
            case SCHED_STATE_WAITING:
                WaitQueue_ResumeOne(self->waiting_on_wait_queue, self);
                break;
            
            default:
                abort();
        }
    }
    preempt_restore(sps);
}

// Returns true if the given virtual processor is currently suspended; false otherwise.
bool VirtualProcessor_IsSuspended(VirtualProcessor* _Nonnull self)
{
    VP_ASSERT_ALIVE(self);
    const int sps = preempt_disable();
    const bool isSuspended = self->suspension_count > 0;
    preempt_restore(sps);
    return isSuspended;
}

// Atomically updates the current signal mask and returns the old mask.
errno_t VirtualProcessor_SetSignalMask(VirtualProcessor* _Nonnull self, int op, sigset_t mask, sigset_t* _Nullable pOutMask)
{
    decl_try_err();
    VP_ASSERT_ALIVE(self);
    const int sps = preempt_disable();
    const sigset_t oldMask = self->sigmask;

    switch (op) {
        case SIG_SETMASK:
            self->sigmask = mask;
            break;

        case SIG_BLOCK:
            self->sigmask |= mask;
            break;

        case SIG_UNBLOCK:
            self->sigmask &= ~mask;
            break;

        default:
            err = EINVAL;
            break;
    }

    if (err == EOK && pOutMask) {
        *pOutMask = oldMask;
    }

    preempt_restore(sps);
    return err;
}

// @Entry Condition: preemption disabled
errno_t VirtualProcessor_Signal(VirtualProcessor* _Nonnull self, int signo)
{
    if (signo < SIGMIN || signo > SIGMAX) {
        return EINVAL;
    }


    const sigset_t sigbit = _SIGBIT(signo);

    self->psigs |= sigbit;
    if ((sigbit & ~self->sigmask) == 0) {
        return false;
    }


    if (self->sched_state == SCHED_STATE_WAITING) {
        WaitQueue_WakeupOne(self->waiting_on_wait_queue, self, WAKEUP_CSW, WRES_SIGNAL);
    }
}
