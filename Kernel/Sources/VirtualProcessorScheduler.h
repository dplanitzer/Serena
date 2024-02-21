//
//  VirtualProcessorScheduler.h
//  Apollo
//
//  Created by Dietmar Planitzer on 2/23/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef VirtualProcessorScheduler_h
#define VirtualProcessorScheduler_h

#include <klib/klib.h>
#include "BootAllocator.h"
#include "SystemDescription.h"
#include "VirtualProcessor.h"


// Set if the context switcher should activate the VP set in 'scheduled' and
// deactivate the VP set in 'running'.
#define CSW_SIGNAL_SWITCH   0x01


// Set if the hardware has a FPU who's state needs to be saved/restored on context switches.
// 68040+ only.
#define CSW_HW_HAS_FPU      0x01


// Set if voluntary context switches are enabled (which is the default). Disabling this will stop wakeup() calls from doing CSWs
#define SCHED_FLAG_VOLUNTARY_CSW_ENABLED   0x01


// The ready queue holds references to all VPs which are ready to run. The queue
// is sorted from highest to lowest priority.
typedef struct _ReadyQueue {
    List    priority[VP_PRIORITY_COUNT];
    uint8_t   populated[VP_PRIORITY_POP_BYTE_COUNT];
} ReadyQueue;


// Note: Keep in sync with lowmem.i
typedef struct _VirtualProcessorScheduler {
    volatile VirtualProcessor* _Nonnull running;                        // Currently running VP
    VirtualProcessor* _Nullable         scheduled;                      // The VP that should be moved to the running state by the context switcher
    VirtualProcessor* _Nonnull          idleVirtualProcessor;           // This VP is scheduled if there is no other VP to schedule
    VirtualProcessor* _Nonnull          bootVirtualProcessor;           // This is the first VP that was created at boot time for a CPU. It takes care of scheduler chores like destroying terminated VPs
    ReadyQueue                          ready_queue;
    volatile uint32_t                     csw_scratch;                    // Used by the CSW to temporarily save A0
    volatile uint8_t                      csw_signals;                    // Signals to the context switcher
    uint8_t                               csw_hw;                         // Hardware characteristics relevant for context switches
    uint8_t                               flags;                          // Scheduler flags
    int8_t                                reserved[1];
    Quantums                            quantums_per_quarter_second;    // 1/4 second in terms of quantums
    List                                timeout_queue;                  // Timeout queue managed by the scheduler. Sorted ascending by timer deadlines
    List                                sleep_queue;                    // VPs which block in a sleep() call wait on this wait queue
    List                                scheduler_wait_queue;           // The scheduler VP waits on this queue
    List                                finalizer_queue;
} VirtualProcessorScheduler;


#define QuantumAllowanceForPriority(pri) \
    ((VP_PRIORITY_HIGHEST - (pri)) >> 3) + 1


extern VirtualProcessorScheduler* _Nonnull gVirtualProcessorScheduler;

// Initializes the virtual processor scheduler and sets up the boot virtual
// processor plus the idle virtual processor. The 'pFunc' function will be
// invoked in the context of the boot virtual processor and it will receive the
// 'pContext' argument. The first context switch from the machine reset context
// to the boot virtual processor context is triggered by calling the
// VirtualProcessorScheduler_IncipientContextSwitch() function. 
extern void VirtualProcessorScheduler_CreateForLocalCPU(SystemDescription* _Nonnull pSysDesc, BootAllocator* _Nonnull pBootAlloc, Closure1Arg_Func _Nonnull pFunc, void* _Nullable _Weak pContext);

extern errno_t VirtualProcessorScheduler_FinishBoot(VirtualProcessorScheduler* _Nonnull pScheduler);
extern _Noreturn VirtualProcessorScheduler_SwitchToBootVirtualProcessor(void);

extern void VirtualProcessorScheduler_AddVirtualProcessor(VirtualProcessorScheduler* _Nonnull pScheduler, VirtualProcessor* _Nonnull pVP);

extern void VirtualProcessorScheduler_OnEndOfQuantum(VirtualProcessorScheduler* _Nonnull pScheduler);

// Put the currently running VP (the caller) on the given wait queue. Then runs
// the scheduler to select another VP to run and context switches to the new VP
// right away.
// Expects to be called with preemption disabled. Temporarily reenables
// preemption when context switching to another VP. Returns to the caller with
// preemption disabled.
extern errno_t VirtualProcessorScheduler_WaitOn(VirtualProcessorScheduler* _Nonnull pScheduler, List* _Nonnull pWaitQueue, TimeInterval deadline, bool isInterruptable);

// Adds the given VP from the given wait queue to the ready queue. The VP is removed
// from the wait queue. The scheduler guarantees that a wakeup operation will never
// fail with an error. This doesn't mean that calling this function will always
// result in a virtual processor wakeup. If the wait queue is empty then no wakeups
// will happen.
extern void VirtualProcessorScheduler_WakeUpOne(VirtualProcessorScheduler* _Nonnull pScheduler, List* _Nonnull pWaitQueue, VirtualProcessor* _Nonnull pVP, int wakeUpReason, bool allowContextSwitch);

// Wakes up up to 'count' waiters on the wait queue 'pWaitQueue'. The woken up
// VPs are removed from the wait queue. Expects to be called with preemption
// disabled.
extern void VirtualProcessorScheduler_WakeUpSome(VirtualProcessorScheduler* _Nonnull pScheduler, List* _Nonnull pWaitQueue, int count, int wakeUpReason, bool allowContextSwitch);

// Adds all VPs on the given list to the ready queue. The VPs are removed from
// the wait queue.
#define VirtualProcessorScheduler_WakeUpAll(__pScheduler, __pWaitQueue, __allowContextSwitch) \
    VirtualProcessorScheduler_WakeUpSome(__pScheduler, __pWaitQueue, INT_MAX, WAKEUP_REASON_FINISHED, __allowContextSwitch);

// Adds all VPs on the given list to the ready queue. The VPs are removed from
// the wait queue. Expects to be called from an interrupt context and thus defers
// context switches until the return from the interrupt context.
extern void VirtualProcessorScheduler_WakeUpAllFromInterruptContext(VirtualProcessorScheduler* _Nonnull pScheduler, List* _Nonnull pWaitQueue);

extern int VirtualProcessorScheduler_DisablePreemption(void);
extern void VirtualProcessorScheduler_RestorePreemption(int sps);

extern int VirtualProcessorScheduler_DisableCooperation(void);
extern void VirtualProcessorScheduler_RestoreCooperation(int sps);
extern int VirtualProcessorScheduler_IsCooperationEnabled(void);

// Gives the virtual processor scheduler opportunities to run tasks that take
// care of internal duties. This function must be called from the boot virtual
// processor. This function does not return to the caller. 
extern _Noreturn VirtualProcessorScheduler_Run(VirtualProcessorScheduler* _Nonnull pScheduler);


//
// The following functions are for use by the VirtualProcessor implementation.
//

// Terminates the given virtual processor that is executing the caller. Does not
// return to the caller. The VP must already have been marked as terminating.
extern _Noreturn VirtualProcessorScheduler_TerminateVirtualProcessor(VirtualProcessorScheduler* _Nonnull pScheduler, VirtualProcessor* _Nonnull pVP);

extern void VirtualProcessorScheduler_AddVirtualProcessor_Locked(VirtualProcessorScheduler* _Nonnull pScheduler, VirtualProcessor* _Nonnull pVP, int effectivePriority);
extern void VirtualProcessorScheduler_RemoveVirtualProcessor_Locked(VirtualProcessorScheduler* _Nonnull pScheduler, VirtualProcessor* _Nonnull pVP);

extern VirtualProcessor* _Nullable VirtualProcessorScheduler_GetHighestPriorityReady(VirtualProcessorScheduler* _Nonnull pScheduler);

extern void VirtualProcessorScheduler_SwitchTo(VirtualProcessorScheduler* _Nonnull pScheduler, VirtualProcessor* _Nonnull pVP);
extern void VirtualProcessorScheduler_MaybeSwitchTo(VirtualProcessorScheduler* _Nonnull pScheduler, VirtualProcessor* _Nonnull pVP);

#endif /* VirtualProcessorScheduler_h */
