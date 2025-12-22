//
//  vcpu.h
//  kernel
//
//  Created by Dietmar Planitzer on 2/21/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef _VCPU_H
#define _VCPU_H 1

#include <hal/clock.h>
#include <hal/cpu.h>
#include <hal/sys_desc.h>
#include <kern/errno.h>
#include <kern/types.h>
#include <klib/Atomic.h>
#include <klib/List.h>
#include <kpi/exception.h>
#include <kpi/signal.h>
#include <kpi/vcpu.h>
#include <sched/stack.h>
#include <sched/waitqueue.h>
#include <sched/sched.h>

struct Process;
struct vcpu;


// Parameters for a VP activation
typedef struct vcpu_acquisition {
    VoidFunc_1 _Nonnull     func;
    void* _Nullable _Weak   arg;
    VoidFunc_0 _Nullable    ret_func;
    void* _Nullable         kernelStackBase;
    size_t                  kernelStackSize;
    size_t                  userStackSize;
    vcpuid_t                id;
    vcpuid_t                groupid;
    sched_params_t          schedParams;
    bool                    isUser;
} vcpu_acquisition_t;

#define VCPU_ACQUISITION_INIT {0}


// VP scheduling state
//
// Possible state transitions:
// INIT         -> SUSPENDED    | READY
// READY        -> RUNNING
// RUNNING      -> READY        | WAITING   | SUSPENDED | TERMINATING
// WAITING      -> READY
// SUSPENDED    -> READY
//
// Entering the SUSPENDED state may be immediate or deferred:
// a) immediate: if a vcpu suspends itself or is in INIT state
// b) deferred: if the vcpu is another user vcpu (not self)
//
// A deferred suspend is delayed until the currently executing system call is
// about to return to user space. A vcpu executing in user space at the time of
// a vcpu_suspend() call is redirected at the next scheduler tick to a
// sigurgent() system call. It will enter the suspended state at the end of this
// system call.
// Note that a system call that is in waiting state at the time a suspension
// request comes in, will continue to wait and the suspension request is left
// pending in the meantime. If the system call receives a wakeup before the
// vcpu has received a resumption request, then the system call will proceed to
// its exit point where it will finally act on the suspension request and suspend
// instead of returning to user space.
// If on the other hand the vcpu is resumed before the wait finished, then the
// vcpu_resume() call atomically clears the suspension request and the system
// call will return directly to user space.
enum {
    SCHED_STATE_INITIATED = 0,  // VP was just created and has not been scheduled yet
    SCHED_STATE_READY,          // VP is able to run and is currently sitting on the ready queue
    SCHED_STATE_RUNNING,        // VP is running
    SCHED_STATE_WAITING,        // VP is blocked waiting for a resource (eg sleep, mutex, semaphore, etc)
    SCHED_STATE_SUSPENDED,      // VP was running or ready and is now suspended (it is not on any queue)
    SCHED_STATE_TERMINATING,    // VP is in the process of terminating and being reaped (it's on the finalizer queue)
};


// VP flags
#define VP_FLAG_HAS_FPU             0x01    // Save/restore the FPU state (keep in sync with machine/hw/m68k/lowmem.i)
#define VP_FLAG_USER_OWNED          0x02    // This VP is owned by a user process
#define VP_FLAG_ACQUIRED            0x04    // vcpu_activate() was called on the VP


#define SCHED_PRIORITY_BIAS_HIGHEST INT8_MAX 
#define SCHED_PRIORITY_BIAS_LOWEST  INT8_MIN 


// Note: Keep in sync with machine/hw/m68k/lowmem.i
struct vcpu {
    ListNode                        rewa_qe;                // A VP is either on the ready (re) queue or a wait (wa) queue

    cpu_savearea_t* _Nonnull        csw_sa;                 // Points to base of the context switcher CPU state save area 
    syscall_savearea_t* _Nullable   syscall_sa;             // Points to base of the system call CPU state save area
    stk_t                           kernel_stack;
    stk_t                           user_stack;

    vcpuid_t                        id;                     // unique VP id (>= 1; 0 is reserved to indicate the absence of a vcpuid; vcpuids are process relative and assigned at acquisition time)
    vcpuid_t                        groupid;                // virtual processor group id. Assigned at acquisition time 

    // VP owner
    ListNode                        owner_qe;               // VP Pool if relinquished; process if acquired

    // System call support
    errno_t                         uerrno;                 // most recent recorded error for user space
    intptr_t                        udata;                  // user space data associated with this VP
    
    // Exceptions support
    excpt_handler_t                 excpt_handler;
    cpu_savearea_t* _Nullable       excpt_sa;               // Exception save area
    int32_t                         excpt_id;               // 0 -> no exception active; > 0 -> exception EXCPT_XXX active

    // Signals
    sigset_t                        pending_sigs;           // Pending signals (sent to the VP, but not yet consumed by sigwait())

    // Waiting related state
    clock_deadline_t                timeout;                // The wait timeout timer
    waitqueue_t _Nullable           waiting_on_wait_queue;  // The wait queue this VP is waiting on; NULL if not waiting. Used by the scheduler to wake up on timeout
    sigset_t                        wait_sigs;              // Which signals should cause a wakeup on arrival
    int8_t                          wakeup_reason;
    
    // Scheduling related state
    int8_t                          qos;                    // call vcpu_sched_params_changed() on change
    int8_t                          qos_priority;
    uint8_t                         reserved2;
    int8_t                          priority_bias;          // used to depress or boost the effective priority (call vcpu_sched_params_changed() on change)
    uint8_t                         sched_priority;         // cached (static) schedule derived from the QoS parameters. Computed by vcpu_sched_params_changed() 
    uint8_t                         effective_priority;     // computed priority used for scheduling. Computed by vcpu_sched_params_changed()
    int8_t                          sched_state;
    uint8_t                         flags;
    int8_t                          quantum_countdown;      // for how many contiguous clock ticks this VP may run for before the scheduler will consider scheduling some other same or lower priority VP
    int16_t                         suspension_count;       // > 0 -> VP is suspended

    // Process
    struct Process* _Nullable _Weak proc;                   // Process owning this VP

    // kdispatch
    void* _Nullable _Weak           dispatch_worker;        // kdispatch_worker if this VP is part of a dispatcher
};


#define vcpu_from_owner_qe(__ptr) \
(vcpu_t) (((uint8_t*)__ptr) - offsetof(struct vcpu, owner_qe))


// Returns a new and unique vcpu group id.
extern vcpuid_t new_vcpu_groupid(void);

// Returns the required minimum kernel stack size
extern size_t min_vcpu_kernel_stack_size(void);


// Acquires a vcpu from the vcpu pool and creates a new vcpu from scratch if none
// is available from the global vcpu pool. The vcpu is configured based on 'ac'.
extern errno_t vcpu_acquire(const vcpu_acquisition_t* _Nonnull ac, vcpu_t _Nonnull * _Nonnull pOutVP);

// Relinquishes a virtual processor which means that it is finished executing
// code and that it should be moved back to the virtual processor pool. This
// function does not return to the caller.
extern _Noreturn vcpu_relinquish(vcpu_t _Nonnull self);


// Returns a reference to the currently running virtual processor. This is the
// virtual processor that is executing the caller.
#define vcpu_current() \
((vcpu_t)(g_sched->running))

// Returns the VPID of the currently running virtual processor.
#define vcpu_currentid() \
((vcpuid_t)(g_sched->running->id))


// Returns true if the vcpu supports user space operations; false if it supports
// kernel space operations only.
#define vcpu_isuser(__self) \
(((__self)->flags & VP_FLAG_USER_OWNED) == VP_FLAG_USER_OWNED)

// Returns a copy of the given virtual processor's scheduling parameters.
extern errno_t vcpu_getschedparams(vcpu_t _Nonnull self, int type, sched_params_t* _Nonnull params);

// Changes the scheduling parameters of the given virtual processor. Does not
// immediately reschedule the VP if it is currently running. Instead the VP is
// allowed to finish its current quanta.
// XXX might want to change that in the future?
extern errno_t vcpu_setschedparams(vcpu_t _Nonnull self, const sched_params_t* _Nonnull params);

// Returns the current (effective) priority of the given VP.
extern int vcpu_getcurrentpriority(vcpu_t _Nonnull self);


// Sends the signal 'signo' to 'self'. The signal is added to the pending signal
// list and the vcpu is woken up if it is currently waiting and the signal
// 'signo' is in the active wake set.
extern errno_t vcpu_sigsend(vcpu_t _Nonnull self, int signo);

// Same as vcpu_sigsend(), but safe to use from a direct interrupt handler.
extern errno_t vcpu_sigsend_irq(vcpu_t _Nonnull self, int signo);

// Returns a copy of the pending signals
extern sigset_t vcpu_sigpending(vcpu_t _Nonnull self);

// Returns true if the vcpu is in aborting state. Meaning that it has received a
// SIGKILL and that it will relinquish soon.
extern bool vcpu_aborting(vcpu_t _Nonnull self);

// @Entry Condition: preemption disabled
extern errno_t vcpu_sigwait(waitqueue_t _Nonnull wq, const sigset_t* _Nonnull set, int* _Nonnull signo);

// @Entry Condition: preemption disabled
extern errno_t vcpu_sigtimedwait(waitqueue_t _Nonnull wq, const sigset_t* _Nonnull set, int flags, const struct timespec* _Nonnull wtp, int* _Nonnull signo);


// Yields the remainder of the current quantum to other VPs.
extern void vcpu_yield(void);


// Suspends the calling virtual processor. This function supports nested calls.
// The following suspend use cases are supported:
// *) a vcpu calls suspend() itself
// *) a vcpu A calls suspend() on a user vcpu B
// Note that involuntary suspension is not supported if the vcpu that should be
// suspended is owned by the kernel process.
// Note that suspension is generically asynchronous. Since teh vcpu that you
// suspend is only able to enter suspended state if it is running user code or
// when it reaches the end of an ongoing system call, it can take a while before
// the vcpu will officially enter suspended state. Suspension related API try
// to mask this as much as possible.
extern errno_t vcpu_suspend(vcpu_t _Nonnull self);

// Resumes the given virtual processor. The virtual processor is forcefully
// resumed if 'force' is true. This means that it is resumed even if the
// suspension count is > 1.
// Resuming a virtual processor is a synchronous operation.
extern void vcpu_resume(vcpu_t _Nonnull self, bool force);

// Read/write the machine context of 'self'. The machine context is the user
// space portion of the vcpu CSW state.
extern errno_t vcpu_rw_mcontext(vcpu_t _Nonnull self, mcontext_t* _Nonnull ctx, bool isRead);


extern void vcpu_dump(vcpu_t _Nonnull self);


//
// Machine integration
//

// Sets the closure which the virtual processor should run when it is next resumed.
extern errno_t _vcpu_reset_mcontext(vcpu_t _Nonnull self, const vcpu_acquisition_t* _Nonnull acq, bool bEnableInterrupts);

// Read/write the CPU context.
extern void _vcpu_write_mcontext(vcpu_t _Nonnull self, const mcontext_t* _Nonnull ctx);
extern void _vcpu_read_mcontext(vcpu_t _Nonnull self, mcontext_t* _Nonnull ctx);

// These functions expect to be called in userspace.
extern void vcpu_uret_relinquish_self(void);
extern void vcpu_uret_exit(void);


//
// Scheduler
//

extern void vcpu_init(vcpu_t _Nonnull self, const sched_params_t* _Nonnull sched_params);

extern void vcpu_destroy(vcpu_t _Nullable self);

// @Entry Condition: preemption disabled
extern void vcpu_reduce_sched_penalty(vcpu_t _Nonnull self, int weight);

// @Entry Condition: preemption disabled
extern void vcpu_sched_params_changed(vcpu_t _Nonnull self);


//
// Syscall
//

extern void vcpu_do_pending_deferred_suspend(vcpu_t _Nonnull self);

#endif /* _VCPU_H */
