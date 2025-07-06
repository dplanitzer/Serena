//
//  VirtualProcessorScheduler.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/23/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "VirtualProcessorScheduler.h"
#include "WaitQueue.h"
#include <hal/InterruptController.h>
#include <hal/MonotonicClock.h>
#include <hal/Platform.h>
#include <kern/timespec.h>
#include <log/Log.h>


extern void VirtualProcessorScheduler_SwitchContext(void);

static void VirtualProcessorScheduler_DumpReadyQueue_Locked(VirtualProcessorScheduler* _Nonnull self);

static VirtualProcessor* _Nonnull BootVirtualProcessor_Create(BootAllocator* _Nonnull bap, VoidFunc_1 _Nonnull fn, void* _Nullable _Weak ctx);
static VirtualProcessor* _Nonnull IdleVirtualProcessor_Create(BootAllocator* _Nonnull bap);


VirtualProcessorScheduler   gVirtualProcessorSchedulerStorage;
VirtualProcessorScheduler*  gVirtualProcessorScheduler = &gVirtualProcessorSchedulerStorage;
WaitQueue                   gSchedulerWaitQueue;           // The scheduler VP waits on this queue


// Initializes the virtual processor scheduler and sets up the boot virtual
// processor plus the idle virtual processor. The 'pFunc' function will be
// invoked in the context of the boot virtual processor and it will receive the
// 'pContext' argument. The first context switch from the machine reset context
// to the boot virtual processor context is triggered by calling the
// VirtualProcessorScheduler_IncipientContextSwitch() function. 
void VirtualProcessorScheduler_CreateForLocalCPU(SystemDescription* _Nonnull sdp, BootAllocator* _Nonnull bap, VoidFunc_1 _Nonnull fn, void* _Nullable _Weak ctx)
{
    // Stored in the BSS. Thus starts out zeroed.
    VirtualProcessorScheduler* self = &gVirtualProcessorSchedulerStorage;


    // Initialize the boot virtual processor
    self->bootVirtualProcessor = BootVirtualProcessor_Create(bap, fn, ctx);


    // Initialize the idle virtual processor
    self->idleVirtualProcessor = IdleVirtualProcessor_Create(bap);


    // Initialize the scheduler
    if (sdp->fpu_model != FPU_MODEL_NONE) {
        self->csw_hw |= CSW_HW_HAS_FPU;
    }
    
    List_Init(&self->timeout_queue);
    WaitQueue_Init(&gSchedulerWaitQueue);
    List_Init(&self->finalizer_queue);

    for (int i = 0; i < VP_PRIORITY_COUNT; i++) {
        List_Init(&self->ready_queue.priority[i]);
    }
    for (int i = 0; i < VP_PRIORITY_POP_BYTE_COUNT; i++) {
        self->ready_queue.populated[i] = 0;
    }
    VirtualProcessorScheduler_AddVirtualProcessor_Locked(
        self,
        self->bootVirtualProcessor,
        self->bootVirtualProcessor->priority);
    
    self->running = NULL;
    self->scheduled = VirtualProcessorScheduler_GetHighestPriorityReady(self);
    self->csw_signals |= CSW_SIGNAL_SWITCH;
    VirtualProcessorScheduler_RemoveVirtualProcessor_Locked(self, self->scheduled);
    
    assert(self->scheduled == self->bootVirtualProcessor);
}

// Called from OnStartup() after the heap has been created. Finishes the scheduler
// initialization.
errno_t VirtualProcessorScheduler_FinishBoot(VirtualProcessorScheduler* _Nonnull self)
{
    decl_try_err();
    struct timespec ts;
    
    timespec_from_ms(&ts, 250);
    self->quantums_per_quarter_second = Quantums_MakeFromTimespec(&ts, QUANTUM_ROUNDING_AWAY_FROM_ZERO);


    // Resume the idle virtual processor
    VirtualProcessor_Resume(self->idleVirtualProcessor, false);
    

    // Hook us up with the quantum timer interrupt
    InterruptHandlerID irqHandler;
    try(InterruptController_AddDirectInterruptHandler(gInterruptController,
                                                  INTERRUPT_ID_QUANTUM_TIMER,
                                                  INTERRUPT_HANDLER_PRIORITY_HIGHEST - 1,
                                                  (InterruptHandler_Closure)VirtualProcessorScheduler_OnEndOfQuantum,
                                                  self,
                                                  &irqHandler));
    InterruptController_SetInterruptHandlerEnabled(gInterruptController, irqHandler, true);

    return EOK;

catch:
    return err;
}

// Adds the given virtual processor with the given effective priority to the
// ready queue and resets its time slice length to the length implied by its
// effective priority.
void VirtualProcessorScheduler_AddVirtualProcessor_Locked(VirtualProcessorScheduler* _Nonnull self, VirtualProcessor* _Nonnull vp, int effectivePriority)
{
    assert(vp != NULL);
    assert(vp->rewa_qe.prev == NULL);
    assert(vp->rewa_qe.next == NULL);
    assert(vp->suspension_count == 0);
    
    vp->sched_state = SCHED_STATE_READY;
    vp->effectivePriority = effectivePriority;
    vp->quantum_allowance = QuantumAllowanceForPriority(vp->effectivePriority);
    vp->wait_start_time = MonotonicClock_GetCurrentQuantums();
    
    List_InsertAfterLast(&self->ready_queue.priority[vp->effectivePriority], &vp->rewa_qe);
    
    const int popByteIdx = vp->effectivePriority >> 3;
    const int popBitIdx = vp->effectivePriority - (popByteIdx << 3);
    self->ready_queue.populated[popByteIdx] |= (1 << popBitIdx);
}

// Adds the given virtual processor to the scheduler and makes it eligible for
// running.
void VirtualProcessorScheduler_AddVirtualProcessor(VirtualProcessorScheduler* _Nonnull self, VirtualProcessor* _Nonnull vp)
{
    // Protect against our scheduling code
    const int sps = preempt_disable();
    
    VirtualProcessorScheduler_AddVirtualProcessor_Locked(self, vp, vp->priority);
    preempt_restore(sps);
}

// Takes the given virtual processor off the ready queue.
void VirtualProcessorScheduler_RemoveVirtualProcessor_Locked(VirtualProcessorScheduler* _Nonnull self, VirtualProcessor* _Nonnull vp)
{
    register const int pri = vp->effectivePriority;
    
    List_Remove(&self->ready_queue.priority[pri], &vp->rewa_qe);
    
    if (List_IsEmpty(&self->ready_queue.priority[pri])) {
        const int i = pri >> 3;
        const int popBitIdx = pri - (i << 3);

        self->ready_queue.populated[i] &= ~(1 << popBitIdx);
    }
}

// Find the best VP to run next and return it. Null is returned if no VP is ready
// to run. This will only happen if this function is called from the quantum
// interrupt while the idle VP is the running VP.
VirtualProcessor* _Nullable VirtualProcessorScheduler_GetHighestPriorityReady(VirtualProcessorScheduler* _Nonnull self)
{
    for (int i = VP_PRIORITY_POP_BYTE_COUNT - 1; i >= 0; i--) {
        register const uint8_t pop = self->ready_queue.populated[i];
        
        if (pop) {
            if (pop & 0x80) { return (VirtualProcessor*) self->ready_queue.priority[(i << 3) + 7].first; }
            if (pop & 0x40) { return (VirtualProcessor*) self->ready_queue.priority[(i << 3) + 6].first; }
            if (pop & 0x20) { return (VirtualProcessor*) self->ready_queue.priority[(i << 3) + 5].first; }
            if (pop & 0x10) { return (VirtualProcessor*) self->ready_queue.priority[(i << 3) + 4].first; }
            if (pop & 0x8)  { return (VirtualProcessor*) self->ready_queue.priority[(i << 3) + 3].first; }
            if (pop & 0x4)  { return (VirtualProcessor*) self->ready_queue.priority[(i << 3) + 2].first; }
            if (pop & 0x2)  { return (VirtualProcessor*) self->ready_queue.priority[(i << 3) + 1].first; }
            if (pop & 0x1)  { return (VirtualProcessor*) self->ready_queue.priority[(i << 3) + 0].first; }
        }
    }
    
    return NULL;
}

// Invoked at the end of every quantum.
void VirtualProcessorScheduler_OnEndOfQuantum(VirtualProcessorScheduler * _Nonnull self)
{
    // First, go through the timeout queue and move all VPs whose timeouts have
    // expired to the ready queue.
    const Quantums now = MonotonicClock_GetCurrentQuantums();
    
    while (self->timeout_queue.first) {
        register Timeout* ct = (Timeout*)self->timeout_queue.first;
        
        if (ct->deadline > now) {
            break;
        }
        
        VirtualProcessor* vp = VP_FROM_TIMEOUT(ct);
        WaitQueue_WakeupOne(vp->waiting_on_wait_queue, vp, 0, WRES_TIMEOUT);
    }
    
    
    // Second, update the time slice info for the currently running VP
    register VirtualProcessor* run = (VirtualProcessor*)self->running;
    
    run->quantum_allowance--;
    if (run->quantum_allowance > 0) {
        return;
    }

    
    // The time slice has expired. Lower our priority and then check whether
    // there's another VP on the ready queue which is more important. If so we
    // context switch to that guy. Otherwise we'll continue to run for another
    // time slice.
    run->effectivePriority = __max(run->effectivePriority - 1, VP_PRIORITY_LOWEST);
    run->quantum_allowance = QuantumAllowanceForPriority(run->effectivePriority);

    register VirtualProcessor* rdy = VirtualProcessorScheduler_GetHighestPriorityReady(self);
    if (rdy == NULL || rdy->effectivePriority <= run->effectivePriority) {
        // We didn't find anything better to run. Continue running the currently
        // running VP.
        return;
    }
    
    
    // Move the currently running VP back to the ready queue and pull the new
    // VP off the ready queue
    VirtualProcessorScheduler_RemoveVirtualProcessor_Locked(self, rdy);
    VirtualProcessorScheduler_AddVirtualProcessor_Locked(self, run, run->priority);

    
    // Request a context switch
    self->scheduled = rdy;
    self->csw_signals |= CSW_SIGNAL_SWITCH;
}

// Inserts the timeout entry of the given vp in the global timeout list at the
// appropriate place.
static void _arm_timeout(VirtualProcessorScheduler* _Nonnull self, VirtualProcessor* _Nonnull vp)
{
    register Timeout* pt = NULL;
    register Timeout* ct = (Timeout*)self->timeout_queue.first;

    while (ct) {
        if (ct->deadline > vp->timeout.deadline) {
            break;
        }
        
        pt = ct;
        ct = (Timeout*)ct->queue_entry.next;
    }
    
    List_InsertAfter(&self->timeout_queue, &vp->timeout.queue_entry, &pt->queue_entry);
}

// Arms a timeout for the given virtual processor. This puts the VP on the timeout
// queue.
void VirtualProcessorScheduler_ArmTimeout(VirtualProcessorScheduler* _Nonnull self, VirtualProcessor* _Nonnull vp, const struct timespec* _Nonnull deadline)
{
    vp->timeout.deadline = Quantums_MakeFromTimespec(deadline, QUANTUM_ROUNDING_AWAY_FROM_ZERO);
    vp->timeout.is_valid = true;
    
    _arm_timeout(self, vp);
}

// Cancels an armed timeout for the given virtual processor. Does nothing if
// no timeout is armed.
void VirtualProcessorScheduler_CancelTimeout(VirtualProcessorScheduler* _Nonnull self, VirtualProcessor* _Nonnull vp)
{
    if (vp->timeout.is_valid) {
        List_Remove(&self->timeout_queue, &vp->timeout.queue_entry);
        vp->timeout.deadline = kQuantums_Infinity;
        vp->timeout.is_valid = false;
    }
}

// Suspends a scheduled timeout for the given virtual processor. Does nothing if
// no timeout is armed.
void VirtualProcessorScheduler_SuspendTimeout(VirtualProcessorScheduler* _Nonnull self, VirtualProcessor* _Nonnull vp)
{
    if (vp->timeout.is_valid) {
        List_Remove(&self->timeout_queue, &vp->timeout.queue_entry);
    }
}

// Resumes a suspended timeout for the given virtual processor.
void VirtualProcessorScheduler_ResumeTimeout(VirtualProcessorScheduler* _Nonnull self, VirtualProcessor* _Nonnull vp, Quantums suspensionTime)
{
    if (vp->timeout.is_valid) {
        vp->timeout.deadline += __max(MonotonicClock_GetCurrentQuantums() - suspensionTime, 0);
        _arm_timeout(self, vp);
    }
}

// Context switches to the given virtual processor if it is a better choice. Eg
// it has a higher priority than the VP that is currently running. This is a
// voluntary (cooperative) context switch which means that it will only happen
// if we are not running in the interrupt context and voluntary context switches
// are enabled.
void VirtualProcessorScheduler_MaybeSwitchTo(VirtualProcessorScheduler* _Nonnull self, VirtualProcessor* _Nonnull vp)
{
    if (vp->sched_state == SCHED_STATE_READY
        && vp->suspension_count == 0) {
        VirtualProcessor* pBestReadyVP = VirtualProcessorScheduler_GetHighestPriorityReady(self);
        
        if (pBestReadyVP == vp && vp->effectivePriority >= self->running->effectivePriority) {
            VirtualProcessor* pCurRunning = (VirtualProcessor*)self->running;
            
            VirtualProcessorScheduler_AddVirtualProcessor_Locked(self, pCurRunning, pCurRunning->priority);
            VirtualProcessorScheduler_SwitchTo(self, vp);
        }
    }
}

// Context switch to the given virtual processor. The VP must be in ready state
// and on the ready queue. Immediately context switches to the VP.
// Expects that the call has already added the currently running VP to a wait
// queue or the finalizer queue.
void VirtualProcessorScheduler_SwitchTo(VirtualProcessorScheduler* _Nonnull self, VirtualProcessor* _Nonnull vp)
{
    VirtualProcessorScheduler_RemoveVirtualProcessor_Locked(self, vp);
    self->scheduled = vp;
    self->csw_signals |= CSW_SIGNAL_SWITCH;
    VirtualProcessorScheduler_SwitchContext();
}

// Terminates the given virtual processor that is executing the caller. Does not
// return to the caller. The VP must already have been marked as terminating.
// @Entry Condition: preemption disabled
_Noreturn VirtualProcessorScheduler_TerminateVirtualProcessor(VirtualProcessorScheduler* _Nonnull self, VirtualProcessor* _Nonnull vp)
{
    List_InsertAfterLast(&self->finalizer_queue, &vp->rewa_qe);
    
    
    // Check whether there are too many VPs on the finalizer queue. If so then we
    // try to context switch to the scheduler VP otherwise we'll context switch
    // to whoever else is the best candidate to run.
    VirtualProcessor* newRunning;
    const int FINALIZE_NOW_THRESHOLD = 4;
    int dead_vps_count = 0;
    ListNode* pCurNode = self->finalizer_queue.first;
    while (pCurNode != NULL && dead_vps_count < FINALIZE_NOW_THRESHOLD) {
        pCurNode = pCurNode->next;
        dead_vps_count++;
    }
    
    if (dead_vps_count >= FINALIZE_NOW_THRESHOLD && gSchedulerWaitQueue.q.first != NULL) {
        // The scheduler VP is currently waiting for work. Let's wake it up.
        WaitQueue_WakeupOne(&gSchedulerWaitQueue,
                        self->bootVirtualProcessor,
                        WAKEUP_CSW,
                        WRES_WAKEUP);
    } else {
        // Do a forced context switch to whoever is ready
        // NOTE: we do NOT put the currently running VP back on the ready queue
        // because it is dead.
        VirtualProcessorScheduler_SwitchTo(self,
                                           VirtualProcessorScheduler_GetHighestPriorityReady(self));
    }
    
    /* NOT REACHED */
}

// Gives the virtual processor scheduler opportunities to run tasks that take
// care of internal duties. This function must be called from the boot virtual
// processor. This function does not return to the caller. 
_Noreturn VirtualProcessorScheduler_Run(VirtualProcessorScheduler* _Nonnull self)
{
    assert(VirtualProcessor_GetCurrent() == self->bootVirtualProcessor);
    List dead_vps;
    struct timespec now, timeout, deadline;

    timespec_from_sec(&timeout, 1);
    
    while (true) {
        List_Init(&dead_vps);
        const int sps = preempt_disable();

        // Continue to wait as long as there's nothing to finalize
        while (List_IsEmpty(&self->finalizer_queue)) {
            (void)WaitQueue_TimedWait(&gSchedulerWaitQueue,
                                &SIGSET_BLOCK_ALL,
                                0,
                                &timeout,
                                NULL);
        }
        
        // Got some work to do. Save off the needed data in local vars and then
        // reenable preemption before we go and do the actual work.
        dead_vps = self->finalizer_queue;
        List_Deinit(&self->finalizer_queue);
        
        preempt_restore(sps);
        
        
        // XXX
        // XXX Should boost the priority of VPs that have been sitting on the
        // XXX ready queue for some time. Goal is to prevent a VP from getting
        // XXX starved to death because it's a very low priority one and there's
        // XXX always some higher priority VP sneaking in. Now a low priority
        // XXX VP which blocks will receive a boost once it is unblocked but this
        // XXX doesn't help background VPs which block rarely or not at all
        // XXX because blocking isn't a natural part of the algorithm they are
        // XXX executing. Eg a VP that scales an image in the background.
        // XXX Boost could be +1 priority every 1/4 second
        // XXX
        
        // Finalize VPs which have exited
        VirtualProcessor* pCurVP = (VirtualProcessor*)dead_vps.first;
        while (pCurVP) {
            VirtualProcessor* pNextVP = (VirtualProcessor*)pCurVP->rewa_qe.next;
            
            VirtualProcessor_Destroy(pCurVP);
            pCurVP = pNextVP;
        }
    }
}

#if 0
static void VirtualProcessorScheduler_DumpReadyQueue_Locked(VirtualProcessorScheduler* _Nonnull self)
{
    for (int i = 0; i < VP_PRIORITY_COUNT; i++) {
        VirtualProcessor* pCurVP = (VirtualProcessor*)self->ready_queue.priority[i].first;
        
        while (pCurVP) {
            printf("{pri: %d}, ", pCurVP->priority);
            pCurVP = (VirtualProcessor*)pCurVP->rewa_qe.next;
        }
    }
    printf("\n");
    for (int i = 0; i < VP_PRIORITY_POP_BYTE_COUNT; i++) {
        printf("%hhx, ", self->ready_queue.populated[i]);
    }
    printf("\n");
}
#endif


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Virtual Processor
////////////////////////////////////////////////////////////////////////////////

VirtualProcessor* VirtualProcessor_GetCurrent(void)
{
    return (VirtualProcessor*) gVirtualProcessorSchedulerStorage.running;
}

int VirtualProcessor_GetCurrentVpid(void)
{
    return gVirtualProcessorSchedulerStorage.running->vpid;
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Boot Virtual Processor
////////////////////////////////////////////////////////////////////////////////


// Initializes a boot virtual processor. This is the virtual processor which
// is used to grandfather in the initial thread of execution at boot time. It is
// the first VP that is created for a physical processor. It then takes over
// duties for the scheduler.
// \param pVP the boot virtual processor record
// \param closure the closure that should be invoked by the virtual processor
static VirtualProcessor* _Nonnull BootVirtualProcessor_Create(BootAllocator* _Nonnull bap, VoidFunc_1 _Nonnull fn, void* _Nullable _Weak ctx)
{
    // Stored in the BSS. Thus starts out zeroed.
    static VirtualProcessor gBootVirtualProcessorStorage;
    VirtualProcessor* vp = &gBootVirtualProcessorStorage;


    // Allocate the boot virtual processor kernel stack
    const int kernelStackSize = CPU_PAGE_SIZE;
    char* pKernelStackBase = BootAllocator_Allocate(bap, kernelStackSize);


    // Create the VP
    VirtualProcessor_CommonInit(vp, VP_PRIORITY_HIGHEST);
    try_bang(VirtualProcessor_SetClosure(vp, VirtualProcessorClosure_MakeWithPreallocatedKernelStack((VoidFunc_1)fn, ctx, pKernelStackBase, kernelStackSize)));
    vp->save_area.sr |= 0x0700;    // IRQs should be disabled by default
    vp->suspension_count = 0;
    
    return vp;
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Idle Virtual Processor
////////////////////////////////////////////////////////////////////////////////

static void IdleVirtualProcessor_Run(void* _Nullable ctx);

// Creates an idle virtual processor. The scheduler schedules this VP if no other
// one is in state ready.
static VirtualProcessor* _Nonnull IdleVirtualProcessor_Create(BootAllocator* _Nonnull bap)
{
        // Stored in the BSS. Thus starts out zeroed.
    static VirtualProcessor gIdleVirtualProcessorStorage;
    VirtualProcessor* vp = &gIdleVirtualProcessorStorage;


    // Allocate the boot virtual processor kernel stack
    const int kernelStackSize = CPU_PAGE_SIZE;
    char* pKernelStackBase = BootAllocator_Allocate(bap, kernelStackSize);


    // Create the VP
    VirtualProcessor_CommonInit(vp, VP_PRIORITY_LOWEST);
    try_bang(VirtualProcessor_SetClosure(vp, VirtualProcessorClosure_MakeWithPreallocatedKernelStack(IdleVirtualProcessor_Run, NULL, pKernelStackBase, kernelStackSize)));

    return vp;
}

// Puts the CPU to sleep until an interrupt occurs. The interrupt will give the
// scheduler a chance to run some other virtual processor if one is ready.
static void IdleVirtualProcessor_Run(void* _Nullable ctx)
{
    while (true) {
        cpu_sleep(gSystemDescription->cpu_model);
    }
}
