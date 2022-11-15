//
//  VirtualProcessorScheduler.h
//  Apollo
//
//  Created by Dietmar Planitzer on 2/23/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef VirtualProcessorScheduler_h
#define VirtualProcessorScheduler_h

#include "Foundation.h"
#include "List.h"
#include "SystemDescription.h"
#include "VirtualProcessor.h"


// Set if the context switcher should activate the VP set in 'running' and
// deactivate the VP set in 'runnerup'.
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
    UInt8   populated[VP_PRIORITY_POP_BYTE_COUNT];
} ReadyQueue;


// Note: Keep in sync with lowmem.i
typedef struct _VirtualProcessorScheduler {
    volatile VirtualProcessor* _Nonnull running;                        // Currently running VP
    VirtualProcessor* _Nullable         scheduled;                      // The VP that should be moved to the running state by the context switcher
    VirtualProcessor* _Nonnull          idleVirtualProcessor;           // THis is VP is scheduled if there is no other VP to schedule
    ReadyQueue                          ready_queue;
    volatile UInt32                     csw_scratch;                    // Used by the CSW to temporarily save A0
    volatile UInt8                      csw_signals;                    // Signals to the context switcher
    UInt8                               csw_hw;                         // Hardware characteristics relevant for context switches
    UInt8                               flags;                          // Scheduler flags
    Int8                                reserved[1];
    Quantums                            quantums_per_quater_second;     // 1/4 second in terms of quantums
    List                                timeout_queue;                  // Timeout queue managed by the scheduler. Sorted descending
    List                                sleep_queue;                    // VPs which use a sleep() call wait on this wait queue
    List                                scheduler_wait_queue;           // The scheduler VP waits on this queue
    List                                finalizer_queue;
} VirtualProcessorScheduler;


extern VirtualProcessorScheduler* _Nonnull VirtualProcessorScheduler_GetShared(void);

extern void VirtualProcessorScheduler_Init(VirtualProcessorScheduler* _Nonnull pScheduler, SystemDescription* _Nonnull pSysDesc, VirtualProcessor* _Nonnull pBootVP);
extern void VirtualProcessorScheduler_FinishBoot(VirtualProcessorScheduler* _Nonnull pScheduler);

extern void VirtualProcessorScheduler_AddVirtualProcessor(VirtualProcessorScheduler* _Nonnull pScheduler, VirtualProcessor* _Nonnull pVP);

extern void VirtualProcessorScheduler_OnEndOfQuantum(VirtualProcessorScheduler* _Nonnull pScheduler);

// Put the currently running VP (the caller) on the given wait queue. Then runs
// the scheduler to select another VP to run and context switches to that new VP
// right away.
// Expects to be called with preemption disabled. Temporarily reenables
// preemption when context switching to another VP. Returns to the caller with
// preemption disabled.
// XXX
// \param isInterruptable true if teh sleep should be interruptable; false otherwise
//        Every sleep should be interruptable. But Lock_Lock() currently isn't because
//        handling interrupted Lock_Lock() is an aboslute pita in C because it has no
//        error model whatsoever...
extern ErrorCode VirtualProcessorScheduler_WaitOn(VirtualProcessorScheduler* _Nonnull pScheduler, List* _Nonnull pWaitQueue, TimeInterval deadline, Bool isInterruptable);

// Adds all VPs on the given list to the ready queue. The VPs are removed from
// the wait queue.
extern ErrorCode VirtualProcessorScheduler_WakeUpAll(VirtualProcessorScheduler* _Nonnull pScheduler, List* _Nonnull pWaitQueue, Bool allowContextSwitch);

// Wakes up up to 'count' waiters on the wait queue 'pWaitQueue'. The woken up
// VPs are removed from the wait queue. Expects to be called with preemption
// disabled.
extern ErrorCode VirtualProcessorScheduler_WakeUpSome(VirtualProcessorScheduler* _Nonnull pScheduler, List* _Nullable pWaitQueue, Int count, Int wakeUpReason, Bool allowContextSwitch);

// Adds the given VP from the given wait queue to the ready queue. The VP is removed
// from the wait queue.
extern ErrorCode VirtualProcessorScheduler_WakeUpOne(VirtualProcessorScheduler* _Nonnull pScheduler, List* _Nullable pWaitQueue, VirtualProcessor* _Nonnull pVP, Int wakeUpReason, Bool allowContextSwitch);

extern Int VirtualProcessorScheduler_DisablePreemption(void);
extern void VirtualProcessorScheduler_RestorePreemption(Int sps);

extern Int VirtualProcessorScheduler_DisableCooperation(void);
extern void VirtualProcessorScheduler_RestoreCooperation(Int sps);
extern Int VirtualProcessorScheduler_IsCooperationEnabled(void);

#endif /* VirtualProcessorScheduler_h */
