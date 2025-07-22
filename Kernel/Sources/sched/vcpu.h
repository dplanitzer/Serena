//
//  vcpu.h
//  kernel
//
//  Created by Dietmar Planitzer on 2/21/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef _VCPU_H
#define _VCPU_H 1

#include <kern/errno.h>
#include <kern/types.h>
#include <klib/Atomic.h>
#include <klib/List.h>
#include <kpi/signal.h>
#include <kpi/vcpu.h>
#include <machine/cpu.h>
#include <machine/SystemDescription.h>
#include <sched/vcpu_stack.h>
#include <sched/waitqueue.h>
#include <sched/sched.h>

struct Process;
struct vcpu;


extern const sigset_t SIGSET_IGNORE_ALL;


// This structure describes a virtual processor closure which is a function entry
// point, a context parameter that will be passed to the closure function and the
// kernel plus user stack size.
typedef struct VirtualProcessorClosure {
    VoidFunc_1 _Nonnull     func;
    void* _Nullable _Weak   context;
    VoidFunc_0 _Nullable    ret_func;
    char* _Nullable         kernelStackBase;    // Optional base address of a pre-allocated kernel stack
    size_t                  kernelStackSize;
    size_t                  userStackSize;
    bool                    isUser;
} VirtualProcessorClosure;


// VP scheduling state
enum {
    SCHED_STATE_READY = 0,      // VP is able to run and is currently sitting on the ready queue
    SCHED_STATE_RUNNING,        // VP is running
    SCHED_STATE_WAITING         // VP is blocked waiting for a resource (eg sleep, mutex, semaphore, etc)
};

// VP lifecycle state
enum {
    VP_LIFECYCLE_RELINQUISHED = 0,  // VP is in the reuse pool
    VP_LIFECYCLE_ACQUIRED,          // VP is assigned to a process and in use
    VP_LIFECYCLE_TERMINATING,       // VP is in the process of terminating
};


// The virtual processor flags

// Minimum size for a kernel stack
#define VP_MIN_KERNEL_STACK_SIZE        16

// Default stack size for kernel space
#define VP_DEFAULT_KERNEL_STACK_SIZE    CPU_PAGE_SIZE

// Minimum size for a user stack
#define VP_MIN_USER_STACK_SIZE          0

// Default stack size for user space
#define VP_DEFAULT_USER_STACK_SIZE      CPU_PAGE_SIZE


// VP flags
#define VP_FLAG_USER_OWNED      0x02    // This VP is owned by a user process


// Overridable functions for virtual processors
struct vcpu_vtable {
    void    (*destroy)(struct vcpu* _Nonnull self);
};
typedef struct vcpu_vtable* vcpu_vtable_t;


// Note: Keep in sync with machine/hal/lowmem.i
struct vcpu {
    ListNode                        rewa_qe;                // A VP is either on the ready (re) queue or a wait (wa) queue
    vcpu_vtable_t _Nonnull          vtable;
    mcontext_t                      save_area;
    vcpu_stack_t                    kernel_stack;
    vcpu_stack_t                    user_stack;
    vcpuid_t                        vpid;                   // unique VP id (>= 1; 0 is reserved to indicate the absence of a VPID)
    vcpuid_t                        vpgid;                  // virtual processor group id. Assigned at acquisition time 

    // VP owner
    ListNode                        owner_qe;               // VP Pool if relinquished; process if acquired

    // System call support
    errno_t                         uerrno;                 // most recent recorded error for user space
    intptr_t                        udata;                  // user space data associated with this VP
    
    // Suspension related state
    Quantums                        suspension_time;        // Absolute time when the VP was suspended

    // Signals
    sigset_t                        pending_sigs;           // Pending signals (sent to the VP, but not yet consumed by sigwait())

    // Waiting related state
    sched_timeout_t                 timeout;                // The timeout state
    waitqueue_t _Nullable           waiting_on_wait_queue;  // The wait queue this VP is waiting on; NULL if not waiting. Used by the scheduler to wake up on timeout
    Quantums                        wait_start_time;        // Time when we entered waiting state
    sigset_t                        wait_sigs;              // Which signals should cause a wakeup on arrival
    int8_t                          wakeup_reason;
    
    // Scheduling related state
    int8_t                          priority;               // base priority
    int8_t                          effectivePriority;      // computed priority used for scheduling
    int8_t                          sched_state;
    uint8_t                         flags;
    int8_t                          quantum_allowance;      // How many continuous quantums this VP may run for before the scheduler will consider scheduling some other VP
    int8_t                          suspension_count;       // > 0 -> VP is suspended

    // Lifecycle state
    int8_t                          lifecycle_state;

    // Process
    struct Process* _Nullable _Weak proc;                   // Process owning this VP (optional for now)

    // Dispatch queue state
    void* _Nullable _Weak           dispatchQueue;                      // Dispatch queue this VP is currently assigned to
    int8_t                          dispatchQueueConcurrencyLaneIndex;  // Index of the concurrency lane in the dispatch queue this VP is assigned to
    int8_t                          reserved2[3];
};


#define VP_ASSERT_ALIVE(p)   assert(p->lifecycle_state != VP_LIFECYCLE_TERMINATING)

#define VP_FROM_OWNER_NODE(__ptr) \
(vcpu_t) (((uint8_t*)__ptr) - offsetof(struct vcpu, owner_qe))

#define VP_FROM_TIMEOUT(__ptr) \
(vcpu_t) (((uint8_t*)__ptr) - offsetof(struct vcpu, timeout))


// Returns a reference to the currently running virtual processor. This is the
// virtual processor that is executing the caller.
extern vcpu_t _Nonnull vcpu_current(void);

// Returns the VPID of the currently running virtual processor.
extern int vcpu_currentid(void);

// Creates a new virtual processor.
// \return the new virtual processor; NULL if creation has failed
extern errno_t vcpu_create(vcpu_t _Nullable * _Nonnull pOutSelf);

void vcpu_destroy(vcpu_t _Nullable self);


// Returns the priority of the given VP.
extern int vcpu_priority(vcpu_t _Nonnull self);

// Changes the priority of a virtual processor. Does not immediately reschedule
// the VP if it is currently running. Instead the VP is allowed to finish its
// current quanta.
// XXX might want to change that in the future?
extern void vcpu_setpriority(vcpu_t _Nonnull self, int priority);


// Atomically updates the current signal mask and returns the old mask.
extern errno_t vcpu_setsigmask(vcpu_t _Nonnull self, int op, sigset_t mask, sigset_t* _Nullable pOutMask);

// @Entry Condition: preemption disabled
extern errno_t vcpu_sendsignal(vcpu_t _Nonnull self, int signo);

// @Entry Condition: preemption disabled
extern errno_t vcpu_sigwait(waitqueue_t _Nonnull wq, const sigset_t* _Nonnull set, int* _Nonnull signo);

// @Entry Condition: preemption disabled
extern errno_t vcpu_sigtimedwait(waitqueue_t _Nonnull wq, const sigset_t* _Nonnull set, int flags, const struct timespec* _Nonnull wtp, int* _Nonnull signo);


// Yields the remainder of the current quantum to other VPs.
extern void vcpu_yield(void);

// Suspends the calling virtual processor. This function supports nested calls.
extern errno_t vcpu_suspend(vcpu_t _Nonnull self);

// Resumes the given virtual processor. The virtual processor is forcefully
// resumed if 'force' is true. This means that it is resumed even if the suspension
// count is > 1.
extern void vcpu_resume(vcpu_t _Nonnull self, bool force);

// Returns true if the given virtual processor is currently suspended; false otherwise.
extern bool vcpu_suspended(vcpu_t _Nonnull self);


// Sets the dispatch queue that has acquired the virtual processor and owns it
// until the virtual processor is relinquished back to the virtual processor
// pool.
extern void vcpu_setdq(vcpu_t _Nonnull self, void* _Nullable pQueue, int concurrencyLaneIndex);

// Sets the closure which the virtual processor should run when it is resumed.
// This function may only be called while the VP is suspended.
extern errno_t vcpu_setclosure(vcpu_t _Nonnull self, const VirtualProcessorClosure* _Nonnull closure);

// Relinquishes the virtual processor which means that it is finished executing
// code and that it should be moved back to the virtual processor pool. This
// function does not return to the caller. This function should only be invoked
// from the bottom-most frame on the virtual processor's kernel stack.
extern _Noreturn vcpu_relinquish(void);

// Terminates the virtual processor that is executing the caller. Does not return
// to the caller. Note that the actual termination of the virtual processor is
// handled by the virtual processor scheduler.
extern _Noreturn vcpu_terminate(vcpu_t _Nonnull self);

extern void vcpu_dump(vcpu_t _Nonnull self);

// These functions expect to be called in userspace.
extern void vcpu_uret_relinquish_self(void);
extern void vcpu_uret_exit(void);


// Subclassers
extern void vcpu_cominit(vcpu_t _Nonnull self, int priority);

extern void __func_vcpu_destroy(vcpu_t _Nullable self);

#endif /* _VCPU_H */
