//
//  sched.h
//  kernel
//
//  Created by Dietmar Planitzer on 2/23/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef _SCHED_H
#define _SCHED_H 1

#include <kern/types.h>
#include <boot/BootAllocator.h>
#include <machine/SystemDescription.h>
#include <kern/errno.h>
#include <kern/limits.h>
#include <klib/List.h>
#include <kpi/vcpu.h>


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


// A timeout
typedef struct sched_timeout {
    ListNode    queue_entry;            // Timeout queue if the VP is waiting with a timeout
    Quantums    deadline;               // Absolute timeout in quantums
    bool        is_valid;               // True if we are waiting with a timeout; false otherwise
    int8_t      reserved[3];
} sched_timeout_t;


// Set if the context switcher should activate the VP set in 'scheduled' and
// deactivate the VP set in 'running'.
#define CSW_SIGNAL_SWITCH   0x01


// Set if the hardware has a FPU who's state needs to be saved/restored on context switches.
// 68040+ only.
#define CSW_HW_HAS_FPU      0x01


// The ready queue holds references to all VPs which are ready to run. The queue
// is sorted from highest to lowest priority.
typedef struct ready_queue {
    List    priority[VP_PRIORITY_COUNT];
    uint8_t populated[VP_PRIORITY_POP_BYTE_COUNT];
} ready_queue_t;


// Note: Keep in sync with machine/hal/lowmem.i
struct sched {
    volatile vcpu_t _Nonnull    running;                        // Currently running VP
    vcpu_t _Nullable            scheduled;                      // The VP that should be moved to the running state by the context switcher
    vcpu_t _Nonnull             idle_vp;                        // This VP is scheduled if there is no other VP to schedule
    vcpu_t _Nonnull             boot_vp;                        // This is the first VP that was created at boot time for a CPU. It takes care of scheduler chores like destroying terminated VPs
    ready_queue_t               ready_queue;
    volatile uint32_t           csw_scratch;                    // Used by the CSW to temporarily save A0
    volatile uint8_t            csw_signals;                    // Signals to the context switcher
    uint8_t                     csw_hw;                         // Hardware characteristics relevant for context switches
    uint8_t                     flags;                          // Scheduler flags
    int8_t                      reserved[1];
    Quantums                    quantums_per_quarter_second;    // 1/4 second in terms of quantums
    List/*<sched_timeout_t>*/   timeout_queue;                  // sched_timeout_t queue managed by the scheduler. Sorted ascending by timer deadlines
    List                        finalizer_queue;
};
typedef struct sched* sched_t;


#define QuantumAllowanceForPriority(__pri) \
    ((VP_PRIORITY_HIGHEST - (__pri)) >> 3) + 1


extern sched_t _Nonnull g_sched;

// Initializes the virtual processor scheduler and sets up the boot virtual
// processor plus the idle virtual processor. The 'pFunc' function will be
// invoked in the context of the boot virtual processor and it will receive the
// 'pContext' argument. The first context switch from the machine reset context
// to the boot virtual processor context is triggered by calling the
// VirtualProcessorScheduler_IncipientContextSwitch() function. 
extern void sched_create(SystemDescription* _Nonnull sdp, BootAllocator* _Nonnull bap, VoidFunc_1 _Nonnull fn, void* _Nullable _Weak ctx);

extern errno_t sched_finish_boot(sched_t _Nonnull self);

extern void sched_add_vcpu(sched_t _Nonnull self, vcpu_t _Nonnull vp);

extern void sched_quantum_irq(sched_t _Nonnull self);

// Arms a timeout for the given virtual processor. This puts the VP on the timeout
// queue.
extern void sched_arm_timeout(sched_t _Nonnull self, vcpu_t _Nonnull vp, const struct timespec* _Nonnull deadline);

// Cancels an armed timeout for the given virtual processor. Does nothing if
// no timeout is armed.
extern void sched_cancel_timeout(sched_t _Nonnull self, vcpu_t _Nonnull vp);

// Gives the virtual processor scheduler opportunities to run tasks that take
// care of internal duties. This function must be called from the boot virtual
// processor. This function does not return to the caller. 
extern _Noreturn sched_run_chores(sched_t _Nonnull self);


//
// The following functions are for use by the vcpu implementation.
//

// Terminates the given virtual processor that is executing the caller. Does not
// return to the caller. The VP must already have been marked as terminating.
// @Entry Condition: preemption disabled
extern _Noreturn sched_terminate_vcpu(sched_t _Nonnull self, vcpu_t _Nonnull vp);

extern void sched_add_vcpu_locked(sched_t _Nonnull self, vcpu_t _Nonnull vp, int effectivePriority);
extern void sched_remove_vcpu_locked(sched_t _Nonnull self, vcpu_t _Nonnull vp);

extern vcpu_t _Nullable sched_highest_priority_ready(sched_t _Nonnull self);

extern void sched_switch_to(sched_t _Nonnull self, vcpu_t _Nonnull vp);
extern void sched_maybe_switch_to(sched_t _Nonnull self, vcpu_t _Nonnull vp);


// Suspends a scheduled timeout for the given virtual processor. Does nothing if
// no timeout is armed.
extern void sched_suspend_timeout(sched_t _Nonnull self, vcpu_t _Nonnull vp);

// Resumes a suspended timeout for the given virtual processor.
extern void sched_resume_timeout(sched_t _Nonnull self, vcpu_t _Nonnull vp, Quantums suspensionTime);

#endif /* _SCHED_H */
