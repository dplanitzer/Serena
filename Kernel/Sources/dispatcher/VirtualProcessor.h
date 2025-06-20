//
//  VirtualProcessor.h
//  kernel
//
//  Created by Dietmar Planitzer on 2/21/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef VirtualProcessor_h
#define VirtualProcessor_h

#include <kern/errno.h>
#include <kern/types.h>
#include <klib/Atomic.h>
#include <klib/List.h>
#include <hal/Platform.h>
#include <hal/SystemDescription.h>


// A kernel or user execution stack
typedef struct ExecutionStack {
    char* _Nullable base;
    size_t          size;
} ExecutionStack;


extern void ExecutionStack_Init(ExecutionStack* _Nonnull pStack);
extern void ExecutionStack_Destroy(ExecutionStack* _Nullable pStack);

extern errno_t ExecutionStack_SetMaxSize(ExecutionStack* _Nullable pStack, size_t size);

#define ExecutionStack_GetInitialTop(__pStack) \
    ((size_t)((__pStack)->base + (__pStack)->size))



// This structure describes a virtual processor closure which is a function entry
// point, a context parameter that will be passed to the closure function and the
// kernel plus user stack size.
typedef struct VirtualProcessorClosure {
    VoidFunc_1 _Nonnull   func;
    void* _Nullable _Weak       context;
    char* _Nullable             kernelStackBase;    // Optional base address of a pre-allocated kernel stack
    size_t                      kernelStackSize;
    size_t                      userStackSize;
} VirtualProcessorClosure;

#define VirtualProcessorClosure_Make(__pFunc, __pContext, __kernelStackSize, __userStackSize) \
    ((VirtualProcessorClosure) {__pFunc, __pContext, NULL, __kernelStackSize, __userStackSize})

// Creates a virtual processor closure with the given function and context parameter.
// The closure will run on a pre-allocated kernel stack. Note that the kernel stack
// must stay allocated until the virtual processor is terminated.
#define VirtualProcessorClosure_MakeWithPreallocatedKernelStack(__pFunc, __pContext, __pKernelStackBase, __kernelStackSize) \
    ((VirtualProcessorClosure) {__pFunc, __pContext, __pKernelStackBase, __kernelStackSize, 0})


// The current state of a virtual processor
typedef enum VirtualProcessorState {
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


// WaitOn() options
#define WAIT_INTERRUPTABLE  1
#define WAIT_ABSTIME        2


// Reason for a wake up
// WAKEUP_REASON_NONE means that we are still waiting for a wake up
#define WAKEUP_REASON_NONE          0
#define WAKEUP_REASON_FINISHED      1
#define WAKEUP_REASON_INTERRUPTED   2
#define WAKEUP_REASON_TIMEOUT       3


struct VirtualProcessor;


// A timeout
typedef struct Timeout {
    ListNode                            queue_entry;            // Timeout queue if the VP is waiting with a timeout
    Quantums                            deadline;               // Absolute timeout in quantums
    struct VirtualProcessor* _Nullable  owner;
    bool                                is_valid;               // True if we are waiting with a timeout; false otherwise
    int8_t                              reserved[3];
} Timeout;


typedef struct VirtualProcessorOwner {
    ListNode                            queue_entry;
    struct VirtualProcessor* _Nonnull   self;
} VirtualProcessorOwner;


// Overridable functions for virtual processors
typedef struct VirtualProcessorVTable {
    void    (* _Nonnull destroy)(struct VirtualProcessor* _Nonnull pVP);
} VirtualProcessorVTable;


// Note: Keep in sync with lowmem.i
typedef struct VirtualProcessor {
    ListNode                                rewa_queue_entry;       // A VP is either on the ready (re) queue or a wait (wa) queue
    const VirtualProcessorVTable* _Nonnull  vtable;
    CpuContext                              save_area;
    ExecutionStack                          kernel_stack;
    ExecutionStack                          user_stack;
    AtomicInt                               vpid;                   // unique VP id (>= 1; 0 is reserved to indicate the absence of a VPID)

    // VP owner
    VirtualProcessorOwner                   owner;

    // System call support
    uint32_t                                syscall_entry_ksp;      // saved Kernel stack pointer at the entry of a system call
    errno_t                                 uerrno;                 // most recent recorded error for user space
    
    // Suspension related state
    Quantums                                suspension_time;        // Absolute time when the VP was suspended
                             
    // Waiting related state
    Timeout                                 timeout;                // The timeout state
    List* _Nullable                         waiting_on_wait_queue;  // The wait queue this VP is waiting on; NULL if not waiting. Used by the scheduler to wake up on timeout
    Quantums                                wait_start_time;        // Time when we entered waiting state
    int8_t                                  wakeup_reason;
    
    // Scheduling related state
    int8_t                                  priority;               // base priority
    int8_t                                  effectivePriority;      // computed priority used for scheduling
    int8_t                                  sched_state;
    uint8_t                                 flags;
    int8_t                                  quantum_allowance;      // How many continuous quantums this VP may run for before the scheduler will consider scheduling some other VP
    int8_t                                  suspension_count;       // > 0 -> VP is suspended
    int8_t                                  reserved[1];

    // Dispatch queue state
    void* _Nullable _Weak                   dispatchQueue;                      // Dispatch queue this VP is currently assigned to
    int8_t                                  dispatchQueueConcurrencyLaneIndex;  // Index of the concurrency lane in the dispatch queue this VP is assigned to
    int8_t                                  reserved2[3];
} VirtualProcessor;


#define VP_ASSERT_ALIVE(p)   assert((p->flags & VP_FLAG_TERMINATED) == 0)


// Returns a reference to the currently running virtual processor. This is the
// virtual processor that is executing the caller.
extern VirtualProcessor* _Nonnull VirtualProcessor_GetCurrent(void);

// Returns the VPID of the currently running virtual processor.
extern int VirtualProcessor_GetCurrentVpid(void);

// Creates a new virtual processor.
// \return the new virtual processor; NULL if creation has failed
extern errno_t VirtualProcessor_Create(VirtualProcessor* _Nullable * _Nonnull pOutSelf);

void VirtualProcessor_Destroy(VirtualProcessor* _Nullable self);


// Sleep for the given number of seconds
extern errno_t VirtualProcessor_Sleep(int options, const struct timespec* _Nonnull wtp, struct timespec* _Nullable rmtp);

// Returns the priority of the given VP.
extern int VirtualProcessor_GetPriority(VirtualProcessor* _Nonnull self);

// Changes the priority of a virtual processor. Does not immediately reschedule
// the VP if it is currently running. Instead the VP is allowed to finish its
// current quanta.
// XXX might want to change that in the future?
extern void VirtualProcessor_SetPriority(VirtualProcessor* _Nonnull self, int priority);

// Returns true if the given virtual processor is currently suspended; false otherwise.
extern bool VirtualProcessor_IsSuspended(VirtualProcessor* _Nonnull self);

// Suspends the calling virtual processor. This function supports nested calls.
extern errno_t VirtualProcessor_Suspend(VirtualProcessor* _Nonnull self);

// Resumes the given virtual processor. The virtual processor is forcefully
// resumed if 'force' is true. This means that it is resumed even if the suspension
// count is > 1.
extern void VirtualProcessor_Resume(VirtualProcessor* _Nonnull self, bool force);

// Sets the dispatch queue that has acquired the virtual processor and owns it
// until the virtual processor is relinquished back to the virtual processor
// pool.
extern void VirtualProcessor_SetDispatchQueue(VirtualProcessor*_Nonnull self, void* _Nullable pQueue, int concurrencyLaneIndex);

// Sets the closure which the virtual processor should run when it is resumed.
// This function may only be called while the VP is suspended.
extern errno_t VirtualProcessor_SetClosure(VirtualProcessor*_Nonnull self, VirtualProcessorClosure closure);

// Invokes the given closure in user space. Preserves the kernel integer register
// state. Note however that this function does not preserve the floating point 
// register state. Call-as-user invocations can not be nested.
extern void VirtualProcessor_CallAsUser(VirtualProcessor* _Nonnull self, VoidFunc_2 _Nonnull func, void* _Nullable context, void* _Nullable arg);

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
extern errno_t VirtualProcessor_AbortCallAsUser(VirtualProcessor*_Nonnull self);

// Relinquishes the virtual processor which means that it is finished executing
// code and that it should be moved back to the virtual processor pool. This
// function does not return to the caller. This function should only be invoked
// from the bottom-most frame on the virtual processor's kernel stack.
extern _Noreturn VirtualProcessor_Relinquish(void);

// Terminates the virtual processor that is executing the caller. Does not return
// to the caller. Note that the actual termination of the virtual processor is
// handled by the virtual processor scheduler.
extern _Noreturn VirtualProcessor_Terminate(VirtualProcessor* _Nonnull self);

extern void VirtualProcessor_Dump(VirtualProcessor* _Nonnull self);

// Subclassers
extern void VirtualProcessor_CommonInit(VirtualProcessor*_Nonnull self, int priority);

extern void __func_VirtualProcessor_Destroy(VirtualProcessor* _Nullable self);

#endif /* VirtualProcessor_h */
