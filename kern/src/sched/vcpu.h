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


// Exception state. Valid if teh vcpu has taken an exception and is in the
// process of handling it. Set up by cpu_exception() and torn down by
// cpu_exception_return().
typedef struct cpu_excpt_state {
    void* _Nonnull  sp;         // user stack pointer at the time of the exception
    void* _Nullable pc;         // program counter at the time of the exception (this is not always the instruction that caused the exception though)
    void* _Nullable fault_addr; // fault address
    int16_t         code;       // platform independent EXCPT_XXX code; if == 0 -> not in exception state; if > 0 -> in exception state
    int16_t         cpu_code;   // corresponding CPU specific code
} cpu_excpt_state_t;


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
// sig_urgent() system call. It will enter the suspended state at the end of this
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


// VP flags (keep in sync with machine/hw/m68k/lowmem.i)
#define VP_FLAG_HAS_FPU             0x01    // Save/restore the FPU state
#define VP_FLAG_HAS_BC              0x02    // Clear branch cache on context switch
// bits 0..3 are reserved for flags that are accessible to C and asm code
#define VP_FLAG_DID_WAIT            0x10    // cleared by default; set when teh vcpu has called wait() at one point while executing the current quantum
#define VP_FLAG_FIXED_PRI           0x20    // set if the vcpu should be scheduled using a fixed priority policy. Derived from the QoS scheduling parameters


#define SCHED_PRIORITY_BIAS_HIGHEST INT8_MAX 
#define SCHED_PRIORITY_BIAS_LOWEST  INT8_MIN 

#define VCPU_CLAMPED_QUANTUM_BOOST(__boost) \
__max(__min(__boost, INT8_MAX), 0)

#define VCPU_CLAMPED_NICE_PRIORITY(__nice) \
__max(__min(__nice, VCPU_QOS_URGENT * VCPU_PRI_COUNT), 0)


// VP tags
#define VP_TAG_SYS  1   /* This VP is owned by  the system/kernel */
#define VP_TAG_USER 2   /* This VP is owned by a user process */
#define VP_TAG_IDLE 3   /* This VP consumes all idle time */


// Note: Keep in sync with machine/hw/m68k/lowmem.i
struct vcpu {
    deque_node_t                    rewa_qe;                // A VP is either on the ready (re) queue or a wait (wa) queue

    cpu_full_state_t* _Nonnull      csw_sa;                 // Points to base of the context switcher CPU state save area 
    cpu_basic_state_t* _Nullable    syscall_sa;             // Points to base of the system call CPU state save area
    stk_t                           kernel_stack;
    stk_t                           user_stack;

    vcpuid_t                        id;                     // unique VP id (>= 1; 0 is reserved to indicate the absence of a vcpuid; vcpuids are process relative and assigned at acquisition time)
    vcpuid_t                        group_id;                // virtual processor group id. Assigned at acquisition time 

    // VP owner
    deque_node_t                    owner_qe;               // VP Pool if relinquished; process if acquired

    // System call support
    errno_t                         uerrno;                 // most recent recorded error for user space
    intptr_t                        udata;                  // user space data associated with this VP
    
    // Exceptions support
    excpt_handler_t                 excpt_handler;          // exception handler and argument
    cpu_full_state_t* _Nullable     excpt_sa;               // Exception save area
    cpu_excpt_state_t               excpt_state;            // Exception state as recorded by cpu_exception()

    // Signals
    volatile sigset_t               pending_sigs;           // Pending signals (sent to the VP, but not yet consumed by vcpu_sigwait())
    volatile sigset_t               wait_sigs;              // Which signals should cause a wakeup on arrival

    // Waiting related state
    clock_deadline_t                timeout;                // The wait timeout timer
    waitqueue_t _Nullable           waiting_on_wait_queue;  // The wait queue this VP is waiting on; NULL if not waiting. Used by the scheduler to wake up on timeout
    int8_t                          wakeup_reason;
    
    // Scheduling related state
    int8_t                          base_priority;          // call vcpu_on_sched_param_changed() on change
    int8_t                          cur_priority;           // computed priority used for scheduling. Computed by vcpu_on_sched_param_changed()
    int8_t                          priority_penalty;       // penalty that should be subtracted from the base priority (call vcpu_on_sched_param_changed() on change)
    int8_t                          priority_boost;         // boost that should be added to the base priority (call vcpu_on_sched_param_changed() on change)
    int8_t                          quantum_boost;
    int8_t                          sched_nice;             // scheduling nice parameter (call vcpu_on_sched_param_changed() on change)
    int8_t                          run_state;
    uint8_t                         flags;
    int8_t                          quantum_countdown;      // for how many contiguous clock ticks this VP may run for before the scheduler will consider scheduling some other same or lower priority VP
    int16_t                         suspension_count;       // > 0 -> VP is suspended
    int8_t                          tag;
    int8_t                          reserved;

    // Usage stats
    nanotime_t                      acquisition_time;
    ticks_t                         user_ticks;
    ticks_t                         system_ticks;
    ticks_t                         wait_ticks;             // accumulated number of ticks spent in waiting or suspended state (since acquisition)

    // Process
    struct Process* _Nullable _Weak proc;                   // Process owning this VP. Note that sched_irq.c assumes that this field is never NULL while the vcpu is acquired and active 

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


// Resets the state of a vcpu. All state is reset to defaults and the provided
// scheduling parameters except the kernel and user stack. You must reset the
// stacks separately.
extern void vcpu_reset(vcpu_t _Nonnull self, const vcpu_policy_t* _Nonnull policy, int nice, int quantum_boost);


// Returns a reference to the currently running virtual processor. This is the
// virtual processor that is executing the caller.
#define vcpu_current() \
((vcpu_t)(g_sched->running))

// Returns the VPID of the currently running virtual processor.
#define vcpu_current_id() \
((vcpuid_t)(g_sched->running->id))


// Returns true if the vcpu supports user space operations; false if it supports
// kernel space operations only.
#define vcpu_is_user(__self) \
((__self)->tag == VP_TAG_USER)

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


// Returns a copy of the receiver's information.
extern errno_t vcpu_info(vcpu_t _Nonnull self, int flavor, vcpu_info_ref _Nonnull info);


// Returns a copy of the pending signals of the current vcpu
extern void vcpu_pending_signals(sigset_t* _Nonnull set);


// Sends the signal 'signo' to 'self'. The signal is added to the pending signal
// list and the vcpu is woken up if it is currently waiting and the signal
// 'signo' is in the active wake set. 'pri_boost' is the QoS priority boost that
// should be granted to 'self'. This should be in the range [0...VCPU_PRI_COUNT-1].
// @IRQ Context Safe
extern errno_t vcpu_send_signal_boost(vcpu_t _Nonnull self, int signo, int pri_boost);

// Same as vcpu_send_signal_boost() without a priority boost.
#define vcpu_send_signal(__self, __signo) \
vcpu_send_signal_boost(__self, __signo, 0)


//#define TIMER_ABSTIME     2      declared in _time.h
#define SIGWAIT_NOABORT     4   /* informs vcpu_sigwait() that it should ignore system call aborts */

// Blocks the caller until either one of the signals in 'set' arrives, a timeout
// happens or an abort signal arrives. Returns EABORTED and leaves the signal
// pending if an abort signal has arrived and SIGWAIT_NOABORT isn't specified.
// Ignores aborting signals altogether if SIGWAIT_NOABORT is specified. If a
// non-aborting signal arrives, that signal is consumed, stored in 'signo' and
// EOK is returned.
// Returns ETIMEDOUT if 'deadline' is an absolute ticks value < TICKS_MAX and
// no signal has arrived before 'deadline'. Use the wq_calc_deadline() function
// to calculate the deadline value based on a nanotime_t timeout. 
extern errno_t vcpu_sigwait(waitqueue_t _Nonnull wq, const sigset_t* _Nonnull set, int flags, const ticks_t deadline, int* _Nonnull signo);

// Return EABORTED if the current vcpu has an aborting signal pending.
// @Entry Condition: preemption disabled
extern errno_t vcpu_testabort_np(void);

// Needs to be called at the end of every system call
extern void vcpu_syscall_epilog(vcpu_t _Nonnull self);


// Returns true if the vcpu is currently in exception mode/state
#define vcpu_is_handling_exception(__self) \
((__self)->excpt_state.code > 0)

extern errno_t vcpu_set_excpt_handler(vcpu_t _Nonnull self, const excpt_handler_t* _Nullable handler, excpt_handler_t* _Nullable old_handler);


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

// Checks whether 'self' is in the process of being suspended or is suspended,
// waits for suspension to have completed and returns EOK. Returns EBUSY if
// 'self' is not in process suspension and not suspended either. 
extern errno_t vcpu_await_suspension(vcpu_t _Nonnull self);


extern errno_t validate_vcpu_policy(const vcpu_policy_t* _Nonnull policy);


//
// Machine integration
//

// Resets the user and kernel stack points back to the base of their respective
// stacks and then pushes the needed frames on the stack so that 'func' will be
// invoked with 'arg' in either user space or kernel space. Depending on whether
// 'isUser' is true or false. 'ret_func' is the address of an argument-less
// function that will be invoked if 'func' returns with an rts instruction.
extern void _vcpu_reset_stacks(vcpu_t _Nonnull self, VoidFunc_1 _Nonnull fsunc, void* _Nullable _Weak arg, VoidFunc_0 _Nonnull ret_func, bool isUser, bool bEnableInterrupts);

// Resets the user stack of the current (calling) vcpu such that the existing
// user stack is wiped out, a frame is pushed with 'arg' as the argument for
// 'func' and 'ret_func' as the return address of 'func'. Then the PC is changed
// to 'func' such that 'func' will be executed when the currently active system
// call returns.
extern void _vcpu_reset_user_stack(vcpu_t _Nonnull self, VoidFunc_1 _Nonnull func, void* _Nullable _Weak arg, VoidFunc_0 _Nonnull ret_func);

extern void _cpu_set_basic_state(cpu_basic_state_t* _Nonnull dp, const vcpu_state_m68k_t* _Nonnull sp);
extern void _cpu_set_float_state(cpu_float_state_t* _Nonnull dp, const vcpu_state_m68k_float_t* _Nonnull sp);
extern void _cpu_set_float_regs(const vcpu_state_m68k_float_t* _Nonnull dp);

extern void _cpu_get_basic_state(vcpu_state_m68k_t* _Nonnull dp, const cpu_basic_state_t* _Nonnull sp);
extern void _cpu_get_float_state(vcpu_state_m68k_float_t* _Nonnull dp, const cpu_float_state_t* _Nonnull sp);
extern void _cpu_get_float_regs(vcpu_state_m68k_float_t* _Nonnull dp);


//
// Scheduler
//

// Initializes 'self' as a vcpu in suspended state. Note that the default QoS
// is BACKGROUND with LOWEST+1 priority. Change the scheduling policy to the
// desired policy by calling vcpu_set_policy() before you resume.
extern void vcpu_init(vcpu_t _Nonnull self);

// Creates a new vcpu in suspended state.
extern errno_t vcpu_create(vcpu_t _Nullable * _Nonnull pOutSelf);

// Destroys the vcpu 'self' and frees all its resources. Note that the vcpu has
// to be in SUSPENDED state.
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
((__self)->cur_priority)

// Returns the effective QoS class.
// @Entry Condition: preemption disabled
#define vcpu_effective_qos_class(__self) \
SCHED_QOS_GRADE((__self)->cur_priority)

// Sets the quantum boost value. The value is clamped to the range 0..INT8_MAX.
// The boost is applied starting with the next quantum.
// @Entry Condition: preemption disabled
extern void vcpu_set_quantum_boost(vcpu_t _Nonnull self, int boost);

// Sets the scheduling nice value. The value is clamped to the range 0..64.
// Note: call vcpu_on_sched_param_changed() next to update the sched state.
// @Entry Condition: preemption disabled
extern void vcpu_set_nice(vcpu_t _Nonnull self, int nice);


//
// Internal
//

extern void _vcpu_do_deferred_suspend_np(vcpu_t _Nonnull self);

#endif /* _VCPU_H */
