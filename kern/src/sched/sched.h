//
//  sched.h
//  kernel
//
//  Created by Dietmar Planitzer on 2/23/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef _SCHED_H
#define _SCHED_H 1

#include <limits.h>
#include <stdint.h>
#include <stdnoreturn.h>
#include <boot/BootAllocator.h>
#include <ext/queue.h>
#include <ext/try.h>
#include <hal/cpu.h>
#include <hal/sys_desc.h>
#include <kpi/vcpu.h>


// Absolute scheduler priorities
#define SCHED_PRI_COUNT     (SCHED_QOS_COUNT * QOS_PRI_COUNT)   /* 80 */
#define SCHED_PRI_HIGHEST   (SCHED_PRI_COUNT-1)                 /* 79 */
#define SCHED_PRI_LOWEST    0                                   /*  0 */

#if defined(__LLP64__)
typedef uint64_t    pop_word_t;
#define SCHED_POP_WORD_WIDTH    64
#define SCHED_POP_SHIFT         6
#define SCHED_POP_MASK          63
#else
typedef uint32_t    pop_word_t;
#define SCHED_POP_WORD_WIDTH    32
#define SCHED_POP_SHIFT         5
#define SCHED_POP_MASK          31
#endif
#define SCHED_POP_WORD_COUNT    ((SCHED_PRI_COUNT + (SCHED_POP_WORD_WIDTH-1)) / SCHED_POP_WORD_WIDTH)


#define SCHED_QUANTUM_SCALE 3
#define SCHED_QUANTUM(__q) (SCHED_QUANTUM_SCALE * (__q))
#define SCHED_QUANTUM_NUDGE 1


// Set if the context switcher should activate the VP set in 'scheduled' and
// deactivate the VP set in 'running'.
#define CSW_SIGNAL_SWITCH       0x01

// Set if the currently running vp has been preempted instead of it finishing its
// quantum or voluntarily blocking
#define CSW_SIGNAL_PREEMPTED    0x02



// The ready queue holds references to all VPs which are ready to run. The queue
// is sorted from highest to lowest priority.
typedef struct ready_queue {
    deque_t     priority[SCHED_PRI_COUNT];
    pop_word_t  populated[SCHED_POP_WORD_COUNT];
} ready_queue_t;

#define sched_rq_mark_populated(__rq, __pri) \
(__rq)->populated[(__pri) >> SCHED_POP_SHIFT] |= (1 << ((__pri) & SCHED_POP_MASK))

#define sched_rq_clear_populated(__rq, __pri) \
(__rq)->populated[(__pri) >> SCHED_POP_SHIFT] &= ~(1 << ((__pri) & SCHED_POP_MASK))



// Note: Keep in sync with machine/hw/m68k/lowmem.i
struct sched {
    volatile vcpu_t _Nullable   running;                        // Currently running VP
    volatile vcpu_t _Nullable   scheduled;                      // The VP that should be moved to the running state by the context switcher
    volatile uint8_t            csw_signals;                    // Signals to the context switcher
    uint8_t                     flags;                          // Scheduler flags
    int8_t                      irq_nest_count;                 // IRQ (routine) nesting count
    int8_t                      reserved;
    vcpu_t _Nonnull             idle_vp;                        // This VP is scheduled if there is no other VP to schedule
    vcpu_t _Nonnull             boot_vp;                        // This is the first VP that was created at boot time for a CPU. It takes care of scheduler chores like destroying terminated VPs
    deque_t                     finalizer_queue;
    ready_queue_t               ready_queue;
};
typedef struct sched* sched_t;


extern const int8_t g_quantum_length[SCHED_QOS_COUNT];
#define qos_quantum(__qos) \
g_quantum_length[__qos]

extern sched_t _Nonnull g_sched;

// Initializes the virtual processor scheduler and sets up the boot virtual
// processor plus the idle virtual processor. The 'fn' function will be
// invoked in the context of the boot virtual processor and it will receive the
// 'ctx' argument. The first context switch from the machine reset context to
// the boot virtual processor context is triggered by calling the
// sched_switch_to_boot_vcpu() function. 
extern void sched_create(BootAllocator* _Nonnull bap, sys_desc_t* _Nonnull sdp, VoidFunc_1 _Nonnull fn, void* _Nullable _Weak ctx);

// Gives the virtual processor scheduler opportunities to run tasks that take
// care of internal duties. This function must be called from the boot virtual
// processor. This function does not return to the caller. 
extern _Noreturn void sched_run_chores(sched_t _Nonnull self);

// Disable and restore preemptive context switches. Scheduling and cooperative
// context switches will continue to function as expected. Also note that if a
// VP A disables preemptive context switches and it then does a cooperative
// context switch to VP B, that VP B will run with preemption enabled.  
extern int preempt_disable(void);
extern void preempt_restore(int sps);


//
// The following functions are for use by the vcpu implementation.
//

// Terminates the given virtual processor that is executing the caller. Does not
// return to the caller. The VP must already have been marked as terminating.
extern _Noreturn void sched_terminate_vcpu(sched_t _Nonnull self, vcpu_t _Nonnull vp);

// Adds 'vp' to the ready queue that corresponds to its effective priority. The
// VP is added to the tail end of the ready queue if 'doAddToTail' is true; to
// the head end if it is false.
// @Entry Condition: preemption disabled
extern void sched_set_ready(sched_t _Nonnull self, vcpu_t _Nonnull vp, bool doAddToTail);

// @Entry Condition: preemption disabled
extern void sched_set_unready(sched_t _Nonnull self, vcpu_t _Nonnull vp, bool doReadyToRun);

// Returns the highest priority vcpu ready for running. NULL if no vcpu is ready
// to run. Note that this happens if the machine is idle, teh idle vcpu is already
// scheduled and all higher priority vcpus are waiting for something. 
// @Entry Condition: preemption disabled
extern vcpu_t _Nullable sched_highest_priority_ready(sched_t _Nonnull self);

extern void sched_switch_to(sched_t _Nonnull self, vcpu_t _Nonnull vp);


// @HAL Requirement: Must be called from the monotonic clock IRQ handler
extern void sched_wait_timeout_irq(vcpu_t _Nonnull vp);

// Invoked at every clock tick and before sched_on_any_irq() is invoked. Runs in
// the interrupt context.
// @HAL Requirement: Must be called from interrupt context
extern void sched_on_tick_irq(sched_t _Nonnull self, excpt_frame_t* _Nonnull efp);

// Invoked at the end of any and all interrupts. Runs in the interrupt context.
// @HAL Requirement: Must be called from interrupt context
extern void sched_on_any_irq(sched_t _Nonnull self, excpt_frame_t* _Nonnull efp);


//
// Scheduler internal functions.
//

// Removes 'vp' from the ready queue and sets it as running VP. Note that this
// function does not move the currently running VP to the ready queue.
// Note that this function does not trigger the actual context switch. The caller
// has to call sched_switch_context() itself. 
// @Entry Condition: preemption disabled
#define sched_set_running(__self, __vp) \
{ \
    (__self)->scheduled = (__vp); \
    (__self)->csw_signals |= CSW_SIGNAL_SWITCH; \
}

// Same as sched_set_running() but marks '__vp' is a preemptor.
// @Entry Condition: preemption disabled
#define sched_set_running_as_preemptor(__self, __vp) \
{ \
    (__self)->scheduled = (__vp); \
    (__self)->csw_signals |= (CSW_SIGNAL_SWITCH|CSW_SIGNAL_PREEMPTED); \
}

// Returns true if the scheduler is currently executing in the interrupt
// context.
#define sched_is_irq_ctx(__self) \
((__self)->irq_nest_count > 0)


#endif /* _SCHED_H */
