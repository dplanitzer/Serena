//
//  vcpu.h
//  kernel
//
//  Created by Dietmar Planitzer on 2/21/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef _VCPU_H
#define _VCPU_H 1

#include <stdint.h>
#include <stddef.h>
#include <stdnoreturn.h>
#include <ext/queue.h>
#include <ext/try.h>
#include <hal/clock.h>
#include <hal/cpu.h>
#include <hal/sys_desc.h>
#include <kpi/exception.h>
#include <kpi/signal.h>
#include <kpi/types.h>
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
    vcpu_policy_t           policy;
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


// VP flags (keep in sync with machine/hw/m68k/lowmem.i)
#define VP_FLAG_HAS_FPU             0x01    // Save/restore the FPU state
#define VP_FLAG_HAS_BC              0x02    // Clear branch cache on context switch
// bits 0..3 are reserved for flags that are accessible to C and asm code
#define VP_FLAG_USER_OWNED          0x10    // This VP is owned by a user process
#define VP_FLAG_ACQUIRED            0x20    // vcpu_activate() was called on the VP
#define VP_FLAG_DID_WAIT            0x40    // cleared by default; set when teh vcpu has called wait() at one point while executing the current quantum
#define VP_FLAG_FIXED_PRI           0x80    // set if the vcpu should be scheduled using a fixed priority policy. Derived from the QoS scheduling parameters


#define SCHED_PRIORITY_BIAS_HIGHEST INT8_MAX 
#define SCHED_PRIORITY_BIAS_LOWEST  INT8_MIN 


// Note: Keep in sync with machine/hw/m68k/lowmem.i
struct vcpu {
    deque_node_t                    rewa_qe;                // A VP is either on the ready (re) queue or a wait (wa) queue

    cpu_full_state_t* _Nonnull      csw_sa;                 // Points to base of the context switcher CPU state save area 
    cpu_basic_state_t* _Nullable    syscall_sa;             // Points to base of the system call CPU state save area
    stk_t                           kernel_stack;
    stk_t                           user_stack;

    vcpuid_t                        id;                     // unique VP id (>= 1; 0 is reserved to indicate the absence of a vcpuid; vcpuids are process relative and assigned at acquisition time)
    vcpuid_t                        groupid;                // virtual processor group id. Assigned at acquisition time 

    // VP owner
    deque_node_t                    owner_qe;               // VP Pool if relinquished; process if acquired

    // System call support
    errno_t                         uerrno;                 // most recent recorded error for user space
    intptr_t                        udata;                  // user space data associated with this VP
    
    // Exceptions support
    excpt_handler_t                 excpt_handler;          // exception handler and argument
    uint32_t                        excpt_handler_flags;    // exception handler flags
    cpu_full_state_t* _Nullable     excpt_sa;               // Exception save area
    int32_t                         excpt_id;               // 0 -> no exception active; > 0 -> exception EXCPT_XXX active

    // Signals
    sigset_t                        pending_sigs;           // Pending signals (sent to the VP, but not yet consumed by sigwait())

    // Waiting related state
    clock_deadline_t                timeout;                // The wait timeout timer
    waitqueue_t _Nullable           waiting_on_wait_queue;  // The wait queue this VP is waiting on; NULL if not waiting. Used by the scheduler to wake up on timeout
    sigset_t                        wait_sigs;              // Which signals should cause a wakeup on arrival
    int8_t                          wakeup_reason;
    
    // Scheduling related state
    int8_t                          base_priority;          // call vcpu_on_sched_param_changed() on change
    int8_t                          effective_priority;     // computed priority used for scheduling. Computed by vcpu_on_sched_param_changed()
    int8_t                          priority_penalty;       // penalty that should be subtracted from the base priority (call vcpu_on_sched_param_changed() on change)
    int8_t                          priority_boost;         // boost that should be added to the base priority (call vcpu_on_sched_param_changed() on change)
    int8_t                          quantum_boost;
    int8_t                          sched_nice;             // scheduling nice parameter (call vcpu_on_sched_param_changed() on change)
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

// Returns the required minimum kernel stack size.
extern size_t min_vcpu_kernel_stack_size(void);

// Invoked by the scheduler at reset time to give the vcpu module a chance to do
// platform specific initialization work.
extern void vcpu_platform_init(void);


// Acquires a vcpu from the vcpu pool and creates a new vcpu from scratch if none
// is available from the global vcpu pool. The vcpu is configured based on 'ac'.
extern errno_t vcpu_acquire(const vcpu_acquisition_t* _Nonnull ac, vcpu_t _Nonnull * _Nonnull pOutVP);

// Relinquishes a virtual processor which means that it is finished executing
// code and that it should be moved back to the virtual processor pool. This
// function does not return to the caller.
extern _Noreturn void vcpu_relinquish(vcpu_t _Nonnull self);


// Returns a reference to the currently running virtual processor. This is the
// virtual processor that is executing the caller.
#define vcpu_current() \
((vcpu_t)(g_sched->running))

// Returns the VPID of the currently running virtual processor.
#define vcpu_current_id() \
((vcpuid_t)(g_sched->running->id))


// Returns true if the vcpu supports user space operations; false if it supports
// kernel space operations only.
#define vcpu_has_user_state(__self) \
(((__self)->flags & VP_FLAG_USER_OWNED) == VP_FLAG_USER_OWNED)

// Returns a copy of the given virtual processor's scheduling policy. 'version'
// specifies which version of the policy structure should be returned. Returns
// EINVAL if the requested version isn't supported.
extern errno_t vcpu_policy(vcpu_t _Nonnull self, int version, vcpu_policy_t* _Nonnull policy);

// Changes the scheduling policy of the given virtual processor. Does not
// immediately reschedule the VP if it is currently running. Instead the VP is
// allowed to finish its current quantum.
extern errno_t vcpu_set_policy(vcpu_t _Nonnull self, const vcpu_policy_t* _Nonnull policy);


// Returns a copy of the receiver's CPU state.
extern errno_t vcpu_state(vcpu_t _Nonnull self, int flavor, vcpu_state_ref _Nonnull state);

// Updates the CPU state of the receiver. If the receiver is not in running state,
// then it must be in suspended state.
extern errno_t vcpu_set_state(vcpu_t _Nonnull self, int flavor, const vcpu_state_ref _Nonnull state);


// Sends the signal 'signo' to 'self'. The signal is added to the pending signal
// list and the vcpu is woken up if it is currently waiting and the signal
// 'signo' is in the active wake set. 'pri_boost' is the QoS priority boost that
// should be granted to 'self'. This should be in the range [0...VCPU_PRI_COUNT-1].
// @IRQ Context Safe
extern errno_t vcpu_send_signal_boost(vcpu_t _Nonnull self, int signo, int pri_boost);

// Same as vcpu_send_signal_boost() without a priority boost.
#define vcpu_send_signal(__self, __signo) \
vcpu_send_signal_boost(__self, __signo, 0)

// Returns a copy of the pending signals
extern sigset_t vcpu_pending_signals(vcpu_t _Nonnull self);

// @Entry Condition: preemption disabled
extern errno_t vcpu_wait_for_signal(waitqueue_t _Nonnull wq, const sigset_t* _Nonnull set, int* _Nonnull signo);

// @Entry Condition: preemption disabled
extern errno_t vcpu_timedwait_for_signal(waitqueue_t _Nonnull wq, const sigset_t* _Nonnull set, int flags, const struct timespec* _Nonnull wtp, int* _Nonnull signo);

// Returns true if the vcpu is in aborting state. Meaning that it has received a
// SIGKILL and that it will relinquish soon.
extern bool vcpu_is_aborting(vcpu_t _Nonnull self);


// Returns true if the vcpu is currently in exception mode/state
#define vcpu_is_handling_exception(__self) \
((__self)->excpt_id > 0)

// Returns a reference to the vcpu's exception handler info if an exception handler
// is assigned to the vcpu. Returns NULL otherwise. Assumes that execution of this
// macro will never overlap with vcpu_set_excpt_handler(). E.g. it is safe to call
// this from cpu_exception() since the vcpu can either process an exception or
// execute vcpu_set_excpt_handler(), but never both at the same time.
#define vcpu_excpt_handler_ref(__self) \
(((__self)->excpt_handler.func) ? &(__self)->excpt_handler : NULL)

extern errno_t vcpu_set_excpt_handler(vcpu_t _Nonnull self, int flags, const excpt_handler_t* _Nullable handler, excpt_handler_t* _Nullable old_handler);


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


extern void vcpu_dump(vcpu_t _Nonnull self);


//
// Machine integration
//

// Sets the closure which the virtual processor should run when it is next resumed.
extern errno_t _vcpu_reset_machine_state(vcpu_t _Nonnull self, const vcpu_acquisition_t* _Nonnull acq, bool bEnableInterrupts);

extern void _cpu_set_basic_state(cpu_basic_state_t* _Nonnull dp, const vcpu_state_m68k_t* _Nonnull sp);
extern void _cpu_set_float_state(cpu_float_state_t* _Nonnull dp, const vcpu_state_m68k_float_t* _Nonnull sp);
extern void _cpu_set_float_regs(const vcpu_state_m68k_float_t* _Nonnull dp);

extern void _cpu_get_basic_state(vcpu_state_m68k_t* _Nonnull dp, const cpu_basic_state_t* _Nonnull sp);
extern void _cpu_get_float_state(vcpu_state_m68k_float_t* _Nonnull dp, const cpu_float_state_t* _Nonnull sp);
extern void _cpu_get_float_regs(vcpu_state_m68k_float_t* _Nonnull dp);

// These functions expect to be called in userspace.
extern void vcpu_uret_relinquish_self(void);
extern void vcpu_uret_exit(void);


//
// Scheduler
//

extern void vcpu_init(vcpu_t _Nonnull self, const vcpu_policy_t* _Nonnull policy);

extern void vcpu_destroy(vcpu_t _Nullable self);

// Indicates that at least one of the receiver's scheduling parameters (QoS,
// priority, boost, penalty, etc) has changed and that the effective priority
// should be recomputed.
// @Entry Condition: preemption disabled
extern void vcpu_on_sched_param_changed(vcpu_t _Nonnull self);

// Resets the receiver's quantum tick count back to the full count for another
// quantum. The quantum length is based on the effective QoS class.
// @Entry Condition: preemption disabled
extern void vcpu_reset_quantum(vcpu_t _Nonnull self);

// Returns true if the vcpu should be scheduled using a fixed priority policy.
// Only valid after vcpu_on_sched_param_changed() has been called
// @Entry Condition: preemption disabled
#define vcpu_is_fixed_pri(__self) \
(((__self)->flags & VP_FLAG_FIXED_PRI) == VP_FLAG_FIXED_PRI)

// Returns the effective (current) priority of the given vcpu.
// @Entry Condition: preemption disabled
#define vcpu_effective_priority(__self) \
((__self)->effective_priority)

// Returns the effective QoS class.
// @Entry Condition: preemption disabled
#define vcpu_effective_qos_class(__self) \
SCHED_QOS_CLASS((__self)->effective_priority)

// Sets the quantum boost value. The value is clamped to the range 0..INT8_MAX.
// The boost is applied starting with the next quantum.
// @Entry Condition: preemption disabled
extern void vcpu_set_quantum_boost(vcpu_t _Nonnull self, int boost);

// Sets the scheduling nice value. The value is clamped to the range 0..64.
// @Entry Condition: preemption disabled
extern void vcpu_set_nice(vcpu_t _Nonnull self, int nice);


//
// Syscall
//

extern void vcpu_do_pending_deferred_suspend(vcpu_t _Nonnull self);

#endif /* _VCPU_H */
