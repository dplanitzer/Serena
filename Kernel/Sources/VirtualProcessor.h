//
//  VirtualProcessor.h
//  Apollo
//
//  Created by Dietmar Planitzer on 2/21/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef VirtualProcessor_h
#define VirtualProcessor_h

#include "Foundation.h"
#include "Atomic.h"
#include "List.h"
#include "MonotonicClock.h"
#include "Platform.h"
#include "SystemDescription.h"


typedef struct _ExecutionStack {
    Byte* _Nullable base;
    Int             size;
} ExecutionStack;


extern ErrorCode ExecutionStack_Init(ExecutionStack* _Nonnull pStack, Int size);
extern void ExecutionStack_Destroy(ExecutionStack* _Nullable pStack);

extern ErrorCode ExecutionStack_SetMaxSize(ExecutionStack* _Nullable pStack, Int size);

static inline Byte* _Nonnull ExecutionStack_GetInitialTop(ExecutionStack* _Nonnull pStack) {
    return pStack->base + pStack->size;
}


// The current state of a virtual processor
typedef enum _VirtualProcessorState {
    kVirtualProcessorState_Ready = 0,       // VP is able to run and is currently sitting on the ready queue
    kVirtualProcessorState_Running,         // VP is running
    kVirtualProcessorState_Waiting,         // VP is blocked waiting for a resource (eg sleep, mutex, semaphore, etc)
    kVirtualProcessorState_Suspended,       // VP is suspended (rewa links are NULL)
} VirtualProcessorState;


// The virtual processor flags

// Default stack size for kernel space
#define VP_DEFAULT_KERNEL_STACK_SIZE    CPU_PAGE_SIZE

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
#define VP_FLAG_INTERRUPTABLE_WAIT  0x01


// Reason for a wake up
// WAKEUP_REASON_NONE means that we are still waiting for a wake up
#define WAKEUP_REASON_NONE          0
#define WAKEUP_REASON_FINISHED      1
#define WAKEUP_REASON_INTERRUPT     2
#define WAKEUP_REASON_TIMEOUT       3


struct _VirtualProcessor;


// Type of the first function a VP will run when it is resumed
typedef void (* _Nonnull VirtualProcessor_Closure)(Byte* _Nullable pContext);


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
    UInt32                                  syscall_entry_ksp;      // saved Kernel stack pointer at the entry of a system call
    AtomicInt                               vpid;                   // unique VP id (>= 1; 0 is reserved to indicate the absence of a VPID)
    
    // VP owner
    VirtualProcessorOwner                   owner;
    
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
} VirtualProcessor;


extern VirtualProcessor* _Nonnull SchedulerVirtualProcessor_GetShared(void);
extern VirtualProcessor* _Nonnull VirtualProcessor_GetCurrent(void);

// Creates a new virtual processor.
// \return the new virtual processor; NULL if creation has failed
extern VirtualProcessor* _Nullable VirtualProcessor_Create(void);

inline void VirtualProcessor_Destroy(VirtualProcessor* _Nullable pVP) {
    if (pVP) {
        pVP->vtable->destroy(pVP);
    }
}

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
extern ErrorCode VirtualProcessor_Resume(VirtualProcessor* _Nonnull pVP, Bool force);

// Sets the max size of the kernel stack. Changing the stack size of a VP is only
// allowed while the VP is suspended. Note that you must call SetClosure() on the
// VP after changing its stack to correctly initialize the stack pointers. A VP
// must always have a kernel stack.
extern ErrorCode VirtualProcessor_SetMaxKernelStackSize(VirtualProcessor*_Nonnull pVP, Int size);

// Sets the max size of the user stack. Changing the stack size of a VP is only
// allowed while the VP is suspended. Note that you must call SetClosure() on the
// VP after changing its stack to correctly initialize the stack pointers. You may
// remove the user stack altogether by passing 0.
extern ErrorCode VirtualProcessor_SetMaxUserStackSize(VirtualProcessor*_Nonnull pVP, Int size);

// Sets the closure which the virtual processor should run when it is resumed.
// This function may only be called while the VP is suspended.
extern void VirtualProcessor_SetClosure(VirtualProcessor*_Nonnull pVP, VirtualProcessor_Closure _Nonnull pClosure, Byte* _Nullable pContext, Bool isUser);

// Reconfigures the execution flow in the given virtual processor such that the
// closure 'pClousre' will be invoked like a subroutine call in user space. The
// interrupted flow of execution will be resumed at the point of interruption
// when 'pClosure' returns. The async call of 'pClosure' is arranged such that:
// 1) if VP is running in user space: 'pClosure' is invoked right away (like a subroutine)
// 2) if the VP is running in kernel space: 'pClosure' is invoked once the currently
//    active system call has completed. 'pClosure' is then called from the syscall
//    RTE and 'pClosure' then returns control back to the original user space code
//    which triggered the system call.
extern ErrorCode VirtualProcessor_ScheduleAsyncUserClosureInvocation(VirtualProcessor*_Nonnull pVP, VirtualProcessor_Closure _Nonnull pClosure, Byte* _Nullable pContext, Bool isNoReturn);

// Exits the calling virtual processor. If possible then the virtual processor is
// moved back to the reuse cache. Otherwise it is destroyed for good.
extern void VirtualProcessor_Exit(VirtualProcessor* _Nonnull pVP);

// Schedules the calling virtual processor for finalization. Does not return.
extern void VirtualProcessor_ScheduleFinalization(VirtualProcessor* _Nonnull pVP);

extern void VirtualProcessor_Dump(VirtualProcessor* _Nonnull pVP);

// Subclassers
extern void VirtualProcessor_CommonInit(VirtualProcessor*_Nonnull pVP, Int priority);

extern void __func_VirtualProcessor_Destroy(VirtualProcessor* _Nullable pVP);


// The scheduler virtual processor
extern void SchedulerVirtualProcessor_Init(VirtualProcessor*_Nonnull pVP, const SystemDescription* _Nonnull pSysDesc, VirtualProcessor_Closure _Nonnull pClosure, Byte* _Nullable pContext);
extern void SchedulerVirtualProcessor_FinishBoot(VirtualProcessor*_Nonnull pVP);

extern void SchedulerVirtualProcessor_Run(Byte* _Nullable pContext);

// The idle virtual processot
extern VirtualProcessor* _Nullable IdleVirtualProcessor_Create(const SystemDescription* _Nonnull pSysDesc);

#endif /* VirtualProcessor_h */
