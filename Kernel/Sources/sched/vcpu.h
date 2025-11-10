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
#include <kpi/exception.h>
#include <kpi/signal.h>
#include <kpi/vcpu.h>
#include <machine/clock.h>
#include <machine/cpu.h>
#include <machine/sys_desc.h>
#include <sched/stack.h>
#include <sched/waitqueue.h>
#include <sched/sched.h>

struct Process;
struct vcpu;


extern const sigset_t SIGSET_IGNORE_ALL;


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
// INIT -> READY
// READY -> RUNNING
// RUNNING -> READY | -> WAITING | -> SUSPENDED | -> TERMINATING
// WAITING -> READY | -> WAIT_SUSPENDED
// SUSPENDED -> READY
//
enum {
    SCHED_STATE_INITIATED = 0,  // VP was just created and has not been scheduled yet
    SCHED_STATE_READY,          // VP is able to run and is currently sitting on the ready queue
    SCHED_STATE_RUNNING,        // VP is running
    SCHED_STATE_WAITING,        // VP is blocked waiting for a resource (eg sleep, mutex, semaphore, etc)
    SCHED_STATE_SUSPENDED,      // VP was running or ready and is now suspended (it is not on any queue)
    SCHED_STATE_WAIT_SUSPENDED, // VP was waiting and is now suspended (it's still on the wait queue it waited on previously)
    SCHED_STATE_TERMINATING,    // VP is in the process of terminating and being reaped (it's on the finalizer queue)
};


// The virtual processor flags

// Default stack size for kernel space
#define VP_DEFAULT_KERNEL_STACK_SIZE    CPU_PAGE_SIZE

// Default stack size for user space
#define VP_DEFAULT_USER_STACK_SIZE      CPU_PAGE_SIZE


// VP flags
#define VP_FLAG_USER_OWNED          0x02    // This VP is owned by a user process
#define VP_FLAG_HANDLING_EXCPT      0x04    // Set while the VP is handling a CPU exception
#define VP_FLAG_HAS_FPU             0x08    // Save/restore the FPU state (keep in sync with lowmem.i)
#define VP_FLAG_FPU_SAVED           0x10    // Set if the FPU user state has been saved (keep in sync with lowmem.i)
#define VP_FLAG_TIMEOUT_SUSPENDED   0x20    // VP is suspended and had an active timeout scheduled
#define VP_FLAG_ACQUIRED            0x40    // vcpu_activate() was called on the VP


#define SCHED_PRIORITY_BIAS_HIGHEST INT8_MAX 
#define SCHED_PRIORITY_BIAS_LOWEST  INT8_MIN 

// Note: Keep in sync with machine/hal/lowmem.i
struct vcpu {
    ListNode                        rewa_qe;                // A VP is either on the ready (re) queue or a wait (wa) queue

    void* _Nonnull                  csw_sa;                 // Points to base of the context switcher CPU state save area 
    void* _Nonnull                  syscall_sa;             // Points to base of the system call CPU state save area
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
    
    // Suspension related state
    tick_t                          suspension_time;        // Absolute time when the VP was suspended

    // Signals
    sigset_t                        pending_sigs;           // Pending signals (sent to the VP, but not yet consumed by sigwait())
    int                             proc_sigs_enabled;      // > 0 means that this VP will accept signals that targeted the process

    // Waiting related state
    clock_deadline_t                timeout;                // The wait timeout timer
    waitqueue_t _Nullable           waiting_on_wait_queue;  // The wait queue this VP is waiting on; NULL if not waiting. Used by the scheduler to wake up on timeout
    sigset_t                        wait_sigs;              // Which signals should cause a wakeup on arrival
    int8_t                          wakeup_reason;
    
    // Scheduling related state
    int8_t                          qos;                    // call vcpu_sched_params_changed() on change
    int8_t                          qos_priority;
    int8_t                          reserved1;
    int8_t                          priority_bias;          // used to depress or boost the effective priority (call vcpu_sched_params_changed() on change)
    uint8_t                         sched_priority;         // cached (static) schedule derived from the QoS parameters. Computed by vcpu_sched_params_changed() 
    uint8_t                         effective_priority;     // computed priority used for scheduling. Computed by vcpu_sched_params_changed()
    int8_t                          sched_state;
    uint8_t                         flags;
    int8_t                          quantum_countdown;      // for how many contiguous clock ticks this VP may run for before the scheduler will consider scheduling some other same or lower priority VP
    int8_t                          suspension_count;       // > 0 -> VP is suspended
    int8_t                          suspension_inhibit_count;   // > 0 -> VP can not be suspended at this time

    // Process
    struct Process* _Nullable _Weak proc;                   // Process owning this VP (optional for now)

    // Dispatch queue state
    void* _Nullable _Weak           dispatchQueue;                      // Dispatch queue this VP is currently assigned to
    int8_t                          dispatchQueueConcurrencyLaneIndex;  // Index of the concurrency lane in the dispatch queue this VP is assigned to
    int8_t                          reserved3[3];
};


#define vcpu_from_owner_qe(__ptr) \
(vcpu_t) (((uint8_t*)__ptr) - offsetof(struct vcpu, owner_qe))


// Returns a new and unique vcpu group id.
extern vcpuid_t new_vcpu_groupid(void);


// Acquires a vcpu from the vcpu pool and creates a new vcpu from scratch if none
// is available from the global vcpu pool. The vcpu is configured based on 'ac'.
extern errno_t vcpu_acquire(const vcpu_acquisition_t* _Nonnull ac, vcpu_t _Nonnull * _Nonnull pOutVP);

// Relinquishes a virtual processor which means that it is finished executing
// code and that it should be moved back to the virtual processor pool. This
// function does not return to the caller.
extern _Noreturn vcpu_relinquish(vcpu_t _Nonnull self);


// Returns a reference to the currently running virtual processor. This is the
// virtual processor that is executing the caller.
extern vcpu_t _Nonnull vcpu_current(void);

// Returns the VPID of the currently running virtual processor.
extern int vcpu_currentid(void);


// Returns a copy of the given virtual processor's scheduling parameters.
extern errno_t vcpu_getschedparams(vcpu_t _Nonnull self, int type, sched_params_t* _Nonnull params);

// Changes the scheduling parameters of the given virtual processor. Does not
// immediately reschedule the VP if it is currently running. Instead the VP is
// allowed to finish its current quanta.
// XXX might want to change that in the future?
extern errno_t vcpu_setschedparams(vcpu_t _Nonnull self, const sched_params_t* _Nonnull params);

// Returns the current (effective) priority of the given VP.
extern int vcpu_getcurrentpriority(vcpu_t _Nonnull self);


// Atomically updates the routing information for process-targeted signals.
extern errno_t vcpu_sigroute(vcpu_t _Nonnull self, int op);

// Forcefully turn process-targeted signal routing off for the given VP.
extern void vcpu_sigrouteoff(vcpu_t _Nonnull self);

// Sends the signal 'signo' to 'self'. The signal is added to the pending signal
// list if either 'isProc' is false or 'isProc' is true and reception of
// process-targeted signals is enabled for 'self'. Otherwise the signal is not
// added to the pending signal list and ignored.
extern errno_t vcpu_sigsend(vcpu_t _Nonnull self, int signo, bool isProc);

// Same as vcpu_sigsend(), but safe to use from a direct interrupt handler.
extern errno_t vcpu_sigsend_irq(vcpu_t _Nonnull self, int signo, bool isProc);

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



// Disable and reenable vcpu_suspend() for the calling virtual processor. These
// calls may be nested. Suspension is only reenabled once all disable calls have
// been balanced by enable calls. A virtual processor will not be suspended as
// long as suspension is inhibited by a suspend_disable() call.
#define vcpu_disable_suspensions(__self) \
(__self)->suspension_inhibit_count++

#define vcpu_enable_suspensions(__self) \
(__self)->suspension_inhibit_count--

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

extern void vcpu_dump(vcpu_t _Nonnull self);


//
// Machine integration
//

// Sets the closure which the virtual processor should run when it is next resumed.
extern errno_t vcpu_setcontext(vcpu_t _Nonnull self, const vcpu_acquisition_t* _Nonnull acq, bool bEnableInterrupts);

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

#endif /* _VCPU_H */
