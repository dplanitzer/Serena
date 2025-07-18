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
#include <machine/MonotonicClock.h>
#include <machine/Platform.h>
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
