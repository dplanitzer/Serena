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
#include <machine/cpu.h>
#include <machine/sys_desc.h>
#include <kern/errno.h>
#include <kern/limits.h>
#include <klib/List.h>
#include <kpi/vcpu.h>


// Absolute scheduler priorities
#define SCHED_PRI_COUNT     ((VCPU_QOS_COUNT-1) * VCPU_PRI_COUNT + 1/*VCPU_QOS_IDLE*/)
#define SCHED_PRI_HIGHEST   (SCHED_PRI_COUNT-1)
#define SCHED_PRI_LOWEST    0

#define SCHED_PRI_POP_BYTE_COUNT    ((SCHED_PRI_COUNT + 7) / 8)


// Set if the context switcher should activate the VP set in 'scheduled' and
// deactivate the VP set in 'running'.
#define CSW_SIGNAL_SWITCH   0x01


// The ready queue holds references to all VPs which are ready to run. The queue
// is sorted from highest to lowest priority.
typedef struct ready_queue {
    List    priority[SCHED_PRI_COUNT];
    uint8_t populated[SCHED_PRI_POP_BYTE_COUNT];
} ready_queue_t;


// Note: Keep in sync with machine/hal/lowmem.i
struct sched {
    volatile vcpu_t _Nonnull    running;                        // Currently running VP
    volatile vcpu_t _Nullable   scheduled;                      // The VP that should be moved to the running state by the context switcher
    volatile uint8_t            csw_signals;                    // Signals to the context switcher
    uint8_t                     flags;                          // Scheduler flags
    int8_t                      reserved[2];
    vcpu_t _Nonnull             idle_vp;                        // This VP is scheduled if there is no other VP to schedule
    vcpu_t _Nonnull             boot_vp;                        // This is the first VP that was created at boot time for a CPU. It takes care of scheduler chores like destroying terminated VPs
    tick_t                      ticks_per_quarter_second;       // 1/4 second in terms of clock ticks
    List                        finalizer_queue;
    ready_queue_t               ready_queue;
};
typedef struct sched* sched_t;


extern const int8_t g_quantum_length[VCPU_QOS_COUNT];
#define qos_quantum(__qos) \
g_quantum_length[__qos]

extern sched_t _Nonnull g_sched;

// Initializes the virtual processor scheduler and sets up the boot virtual
// processor plus the idle virtual processor. The 'fn' function will be
// invoked in the context of the boot virtual processor and it will receive the
// 'ctx' argument. The first context switch from the machine reset context to
// the boot virtual processor context is triggered by calling the
// csw_switch_to_boot_vcpu() function. 
extern void sched_create(BootAllocator* _Nonnull bap, sys_desc_t* _Nonnull sdp, VoidFunc_1 _Nonnull fn, void* _Nullable _Weak ctx);

// Called from OnStartup() after the heap has been created. Finishes the scheduler
// initialization.
extern void sched_finish_boot(sched_t _Nonnull self);

// Adds the given virtual processor with the given effective priority to the
// ready queue and resets its time slice length to the length implied by its
// effective priority.
extern void sched_add_vcpu(sched_t _Nonnull self, vcpu_t _Nonnull vp);

// Gives the virtual processor scheduler opportunities to run tasks that take
// care of internal duties. This function must be called from the boot virtual
// processor. This function does not return to the caller. 
extern _Noreturn sched_run_chores(sched_t _Nonnull self);

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
extern _Noreturn sched_terminate_vcpu(sched_t _Nonnull self, vcpu_t _Nonnull vp);

extern void sched_add_vcpu_locked(sched_t _Nonnull self, vcpu_t _Nonnull vp, int effectivePriority);
extern void sched_remove_vcpu_locked(sched_t _Nonnull self, vcpu_t _Nonnull vp);

extern vcpu_t _Nullable sched_highest_priority_ready(sched_t _Nonnull self);

extern void sched_switch_to(sched_t _Nonnull self, vcpu_t _Nonnull vp);
extern void sched_maybe_switch_to(sched_t _Nonnull self, vcpu_t _Nonnull vp);


// @HAL Requirement: Must be called from the monotonic clock IRQ handler
extern void sched_wait_timeout_irq(vcpu_t _Nonnull vp);

// @HAL Requirement: Must be called from the monotonic clock IRQ handler
extern void sched_tick_irq(sched_t _Nonnull self, excpt_frame_t* _Nonnull efp);

#endif /* _SCHED_H */
