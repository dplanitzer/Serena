//
//  VirtualProcessorScheduler.h
//  kernel
//
//  Created by Dietmar Planitzer on 2/23/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef VirtualProcessorScheduler_h
#define VirtualProcessorScheduler_h

#include <kern/types.h>
#include <boot/BootAllocator.h>
#include <machine/SystemDescription.h>
#include <kern/limits.h>
#include <sched/vcpu.h>


// Set if the context switcher should activate the VP set in 'scheduled' and
// deactivate the VP set in 'running'.
#define CSW_SIGNAL_SWITCH   0x01


// Set if the hardware has a FPU who's state needs to be saved/restored on context switches.
// 68040+ only.
#define CSW_HW_HAS_FPU      0x01


// The ready queue holds references to all VPs which are ready to run. The queue
// is sorted from highest to lowest priority.
typedef struct ReadyQueue {
    List    priority[VP_PRIORITY_COUNT];
    uint8_t populated[VP_PRIORITY_POP_BYTE_COUNT];
} ReadyQueue;


// Note: Keep in sync with machine/hal/lowmem.i
typedef struct VirtualProcessorScheduler {
    volatile vcpu_t _Nonnull    running;                        // Currently running VP
    vcpu_t _Nullable            scheduled;                      // The VP that should be moved to the running state by the context switcher
    vcpu_t _Nonnull             idleVirtualProcessor;           // This VP is scheduled if there is no other VP to schedule
    vcpu_t _Nonnull             bootVirtualProcessor;           // This is the first VP that was created at boot time for a CPU. It takes care of scheduler chores like destroying terminated VPs
    ReadyQueue                  ready_queue;
    volatile uint32_t           csw_scratch;                    // Used by the CSW to temporarily save A0
    volatile uint8_t            csw_signals;                    // Signals to the context switcher
    uint8_t                     csw_hw;                         // Hardware characteristics relevant for context switches
    uint8_t                     flags;                          // Scheduler flags
    int8_t                      reserved[1];
    Quantums                    quantums_per_quarter_second;    // 1/4 second in terms of quantums
    List                        timeout_queue;                  // clock_timeout_t queue managed by the scheduler. Sorted ascending by timer deadlines
    List                        finalizer_queue;
} VirtualProcessorScheduler;


#define QuantumAllowanceForPriority(__pri) \
    ((VP_PRIORITY_HIGHEST - (__pri)) >> 3) + 1


extern VirtualProcessorScheduler* _Nonnull gVirtualProcessorScheduler;

// Initializes the virtual processor scheduler and sets up the boot virtual
// processor plus the idle virtual processor. The 'pFunc' function will be
// invoked in the context of the boot virtual processor and it will receive the
// 'pContext' argument. The first context switch from the machine reset context
// to the boot virtual processor context is triggered by calling the
// VirtualProcessorScheduler_IncipientContextSwitch() function. 
extern void VirtualProcessorScheduler_CreateForLocalCPU(SystemDescription* _Nonnull sdp, BootAllocator* _Nonnull bap, VoidFunc_1 _Nonnull fn, void* _Nullable _Weak ctx);

extern errno_t VirtualProcessorScheduler_FinishBoot(VirtualProcessorScheduler* _Nonnull self);
extern _Noreturn VirtualProcessorScheduler_SwitchToBootVirtualProcessor(void);

extern void VirtualProcessorScheduler_AddVirtualProcessor(VirtualProcessorScheduler* _Nonnull self, vcpu_t _Nonnull vp);

extern void VirtualProcessorScheduler_OnEndOfQuantum(VirtualProcessorScheduler* _Nonnull self);

// Arms a timeout for the given virtual processor. This puts the VP on the timeout
// queue.
extern void VirtualProcessorScheduler_ArmTimeout(VirtualProcessorScheduler* _Nonnull self, vcpu_t _Nonnull vp, const struct timespec* _Nonnull deadline);

// Cancels an armed timeout for the given virtual processor. Does nothing if
// no timeout is armed.
extern void VirtualProcessorScheduler_CancelTimeout(VirtualProcessorScheduler* _Nonnull self, vcpu_t _Nonnull vp);

// Gives the virtual processor scheduler opportunities to run tasks that take
// care of internal duties. This function must be called from the boot virtual
// processor. This function does not return to the caller. 
extern _Noreturn VirtualProcessorScheduler_Run(VirtualProcessorScheduler* _Nonnull self);


//
// The following functions are for use by the vcpu implementation.
//

// Terminates the given virtual processor that is executing the caller. Does not
// return to the caller. The VP must already have been marked as terminating.
// @Entry Condition: preemption disabled
extern _Noreturn VirtualProcessorScheduler_TerminateVirtualProcessor(VirtualProcessorScheduler* _Nonnull self, vcpu_t _Nonnull vp);

extern void VirtualProcessorScheduler_AddVirtualProcessor_Locked(VirtualProcessorScheduler* _Nonnull self, vcpu_t _Nonnull vp, int effectivePriority);
extern void VirtualProcessorScheduler_RemoveVirtualProcessor_Locked(VirtualProcessorScheduler* _Nonnull self, vcpu_t _Nonnull vp);

extern vcpu_t _Nullable VirtualProcessorScheduler_GetHighestPriorityReady(VirtualProcessorScheduler* _Nonnull self);

extern void VirtualProcessorScheduler_SwitchTo(VirtualProcessorScheduler* _Nonnull self, vcpu_t _Nonnull vp);
extern void VirtualProcessorScheduler_MaybeSwitchTo(VirtualProcessorScheduler* _Nonnull self, vcpu_t _Nonnull vp);


// Suspends a scheduled timeout for the given virtual processor. Does nothing if
// no timeout is armed.
extern void VirtualProcessorScheduler_SuspendTimeout(VirtualProcessorScheduler* _Nonnull self, vcpu_t _Nonnull vp);

// Resumes a suspended timeout for the given virtual processor.
extern void VirtualProcessorScheduler_ResumeTimeout(VirtualProcessorScheduler* _Nonnull self, vcpu_t _Nonnull vp, Quantums suspensionTime);

#endif /* VirtualProcessorScheduler_h */
