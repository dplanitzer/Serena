//
//  VirtualProcessor.h
//  Apollo
//
//  Created by Dietmar Planitzer on 2/21/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef VirtualProcessor_h
#define VirtualProcessor_h

#include <klib/klib.h>
#include "MonotonicClock.h"
#include "Platform.h"
#include "SystemDescription.h"


// A kernel or user execution stack
typedef struct _ExecutionStack {
    Byte* _Nullable base;
    Int             size;
} ExecutionStack;


extern void ExecutionStack_Init(ExecutionStack* _Nonnull pStack);
extern void ExecutionStack_Destroy(ExecutionStack* _Nullable pStack);

extern ErrorCode ExecutionStack_SetMaxSize(ExecutionStack* _Nullable pStack, Int size);

static inline Byte* _Nonnull ExecutionStack_GetInitialTop(ExecutionStack* _Nonnull pStack) {
    return pStack->base + pStack->size;
}


// This structure describes a virtual processor closure which is a function entry
// point, a context parameter that will be passed to the closure function and the
// kernel plus user stack size.
typedef struct _VirtualProcessorClosure {
    Closure1Arg_Func _Nonnull   func;
    Byte* _Nullable _Weak       context;
    Byte* _Nullable             kernelStackBase;    // Optional base address of a pre-allocated kernel stack
    Int                         kernelStackSize;
    Int                         userStackSize;
} VirtualProcessorClosure;

static inline VirtualProcessorClosure VirtualProcessorClosure_Make(Closure1Arg_Func _Nonnull pFunc, Byte* _Nullable _Weak pContext, Int kernelStackSize, Int userStackSize) {
    VirtualProcessorClosure c;
    c.func = pFunc;
    c.context = pContext;
    c.kernelStackBase = NULL;
    c.kernelStackSize = kernelStackSize;
    c.userStackSize = userStackSize;
    return c;
}

// Creates a virtua processor closure with the given function and context parameter.
// The closure will run on a pre-allocated kernel stack. Note that the kernel stack
// must stay allocated until the virtual processor is terminated.
static inline VirtualProcessorClosure VirtualProcessorClosure_MakeWithPreallocatedKernelStack(Closure1Arg_Func _Nonnull pFunc, Byte* _Nullable _Weak pContext, Byte* _Nonnull pKernelStackBase, Int kernelStackSize) {
    VirtualProcessorClosure c;
    c.func = pFunc;
    c.context = pContext;
    c.kernelStackBase = pKernelStackBase;
    c.kernelStackSize = kernelStackSize;
    c.userStackSize = 0;
    return c;
}


// The current state of a virtual processor
typedef enum _VirtualProcessorState {
    kVirtualProcessorState_Ready = 0,       // VP is able to run and is currently sitting on the ready queue
    kVirtualProcessorState_Running,         // VP is running
    kVirtualProcessorState_Waiting          // VP is blocked waiting for a resource (eg sleep, mutex, semaphore, etc)
} VirtualProcessorState;


// The virtual processor flags

// Minimum size for a kernel stack
#define VP_MIN_KERNEL_STACK_SIZE        16

// Default stack size for kernel space
#define VP_DEFAULT_KERNEL_STACK_SIZE    CPU_PAGE_SIZE

// Minimum size for a user stack
#define VP_MIN_USER_STACK_SIZE          0

// Default stack size for user space
#define VP_DEFAULT_USER_STACK_SIZE      CPU_PAGE_SIZE


// Virtual processor priorities
#define VP_PRIORITY_HIGHEST     63
#define VP_PRIORITY_REALTIME    56
#define VP_PRIORITY_NORMAL      42
#define VP_PRIORITY_LOWEST      0

#define VP_PRIORITY_COUNT       64
#define VP_PRIORITY_POP_BYTE_COUNT  ((VP_PRIORITY_COUNT + 7) / 8)


// The top 2 and the bottom 2 priorities are reserved for the scheduler
#define VP_PRIORITIES_RESERVED_HIGH 2
#define VP_PRIORITIES_RESERVED_LOW  2


// VP flags
#define VP_FLAG_TERMINATED          0x01    // VirtualProcessor_Terminate() was called on the VP
#define VP_FLAG_CAU_IN_PROGRESS     0x02    // VirtualProcessor_CallAsUser() is in progress
#define VP_FLAG_CAU_ABORTED         0x04    // VirtualProcessor_AbortCallAsUser() has been called and the VirtualProcessor_CallAsUser() is unwinding
#define VP_FLAG_INTERRUPTABLE_WAIT  0x08    // VirtualProcessorScheduler_WaitOn() should be interruptable


// Reason for a wake up
// WAKEUP_REASON_NONE means that we are still waiting for a wake up
#define WAKEUP_REASON_NONE          0
#define WAKEUP_REASON_FINISHED      1
#define WAKEUP_REASON_INTERRUPTED   2
#define WAKEUP_REASON_TIMEOUT       3


struct _VirtualProcessor;


// A timeout
typedef struct _Timeout {
    ListNode                            queue_entry;            // Timeout queue if the VP is waiting with a timeout
    Quantums                            deadline;               // Absolute timeout in quantums
    struct VirtualProcessor* _Nullable  owner;
    Bool                                is_valid;               // True if we are waiting with a timeout; false otherwise
    Int8                                reserved[3];
} Timeout;


typedef struct _VirtualProcessorOwner {
    ListNode                            queue_entry;
    struct _VirtualProcessor* _Nonnull  self;
} VirtualProcessorOwner;


// Overridable functions for virtual processors
typedef struct _VirtualProcessorVTable {
    void    (* _Nonnull destroy)(struct _VirtualProcessor* _Nonnull pVP);
} VirtualProcessorVTable;


// Note: Keep in sync with lowmem.i
typedef struct _VirtualProcessor {
    ListNode                                rewa_queue_entry;       // A VP is either on the ready (re) queue or a wait (wa) queue
    const VirtualProcessorVTable* _Nonnull  vtable;
    CpuContext                              save_area;
    ExecutionStack                          kernel_stack;
    ExecutionStack                          user_stack;
    AtomicInt                               vpid;                   // unique VP id (>= 1; 0 is reserved to indicate the absence of a VPID)

    // VP owner
    VirtualProcessorOwner                   owner;

    // System call support
    UInt32                                  syscall_entry_ksp;      // saved Kernel stack pointer at the entry of a system call
    
    // Waiting related state
    Timeout                                 timeout;                // The timeout state
    List* _Nullable                         waiting_on_wait_queue;  // The wait queue this VP is waiting on; NULL if not waiting. Used by the scheduler to wake up on timeout
    Quantums                                wait_start_time;        // Time when we entered waiting state
    Int8                                    wakeup_reason;
    
    // Scheduling related state
    Int8                                    priority;               // base priority
    Int8                                    effectivePriority;      // computed priority used for scheduling
    UInt8                                   state;
    UInt8                                   flags;
    Int8                                    quantum_allowance;      // How many continuous quantums this VP may run for before the scheduler will consider scheduling some other VP
    Int8                                    suspension_count;       // > 0 -> VP is suspended
    Int8                                    reserved[1];

    // Dispatch queue state
    void* _Nullable _Weak                   dispatchQueue;                      // Dispatch queue this VP is currently assigned to
    Int8                                    dispatchQueueConcurrencyLaneIndex;  // Index of the concurrency lane in the dispatch queue this VP is assigned to
    Int8                                    reserved2[3];
} VirtualProcessor;


#define VP_ASSERT_ALIVE(p)   assert((p->flags & VP_FLAG_TERMINATED) == 0)


// Returns a reference to the currently running virtual processor. This is the
// virtual processor that is executing the caller.
extern VirtualProcessor* _Nonnull VirtualProcessor_GetCurrent(void);

// Returns the VPID of the currently running virtual processor.
extern Int VirtualProcessor_GetCurrentVpid(void);

// Creates a new virtual processor.
// \return the new virtual processor; NULL if creation has failed
extern ErrorCode VirtualProcessor_Create(VirtualProcessor* _Nullable * _Nonnull pOutVP);

void VirtualProcessor_Destroy(VirtualProcessor* _Nullable pVP);


// Sleep for the given number of seconds
extern ErrorCode VirtualProcessor_Sleep(TimeInterval delay);

// Returns the priority of the given VP.
extern Int VirtualProcessor_GetPriority(VirtualProcessor* _Nonnull pVP);

// Changes the priority of a virtual processor. Does not immediately reschedule
// the VP if it is currently running. Instead the VP is allowed to finish its
// current quanta.
// XXX might want to change that in the future?
extern void VirtualProcessor_SetPriority(VirtualProcessor* _Nonnull pVP, Int priority);

// Returns true if the given virtual processor is currently suspended; false otherwise.
extern Bool VirtualProcessor_IsSuspended(VirtualProcessor* _Nonnull pVP);

// Suspends the calling virtual processor. This function supports nested calls.
extern ErrorCode VirtualProcessor_Suspend(VirtualProcessor* _Nonnull pVP);

// Resumes the given virtual processor. The virtual processor is forcefully
// resumed if 'force' is true. This means that it is resumed even if the suspension
// count is > 1.
extern void VirtualProcessor_Resume(VirtualProcessor* _Nonnull pVP, Bool force);

// Sets the dispatch queue that has acquired the virtual processor and owns it
// until the virtual processor is relinquished back to the virtual processor
// pool.
extern void VirtualProcessor_SetDispatchQueue(VirtualProcessor*_Nonnull pVP, void* _Nullable pQueue, Int concurrencyLaneIndex);

// Sets the closure which the virtual processor should run when it is resumed.
// This function may only be called while the VP is suspended.
extern ErrorCode VirtualProcessor_SetClosure(VirtualProcessor*_Nonnull pVP, VirtualProcessorClosure closure);

// Invokes the given closure in user space. Preserves the kernel integer register
// state. Note however that this function does not preserve the floating point 
// register state. Call-as-user invocations can not be nested.
extern void VirtualProcessor_CallAsUser(VirtualProcessor* _Nonnull pVP, Closure1Arg_Func _Nonnull pClosure, Byte* _Nullable pContext);

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
//                          made to unwind the userspace stack or to free any
//                          userspace resources.
// 2) executing a system call: the system call is allowed to run to completion.
//                          However all interruptable waits will be interrupted
//                          no matter whether the VP is currently sitting in an
//                          interruptable wait or it enters it. This behavior
//                          will stay in effect until the VP has returned from
//                          the system. Once the system call has finished and the
//                          call-as-user invocation has been aborted, waits will
//                          not be interrupted anymore.
extern ErrorCode VirtualProcessor_AbortCallAsUser(VirtualProcessor*_Nonnull pVP);

// Relinquishes the virtual processor which means that it is finished executing
// code and that it should be moved back to the virtual processor pool. This
// function does not return to the caller. This function should only be invoked
// from the bottom-most frame on the virtual processor's kernel stack.
extern _Noreturn VirtualProcesssor_Relinquish(void);

// Terminates the virtual processor that is executing the caller. Does not return
// to the caller. Note that the actual termination of the virtual processor is
// handled by the virtual processor scheduler.
extern _Noreturn VirtualProcessor_Terminate(VirtualProcessor* _Nonnull pVP);

extern void VirtualProcessor_Dump(VirtualProcessor* _Nonnull pVP);

// Subclassers
extern void VirtualProcessor_CommonInit(VirtualProcessor*_Nonnull pVP, Int priority);

extern void __func_VirtualProcessor_Destroy(VirtualProcessor* _Nullable pVP);

#endif /* VirtualProcessor_h */
