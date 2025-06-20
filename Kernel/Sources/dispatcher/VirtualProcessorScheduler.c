//
//  VirtualProcessorScheduler.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/23/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "VirtualProcessorScheduler.h"
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
    List_Init(&self->sleep_queue);
    List_Init(&self->scheduler_wait_queue);
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
    self->flags |= SCHED_FLAG_VOLUNTARY_CSW_ENABLED;
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
    assert(vp->rewa_queue_entry.prev == NULL);
    assert(vp->rewa_queue_entry.next == NULL);
    assert(vp->suspension_count == 0);
    
    vp->sched_state = kVirtualProcessorState_Ready;
    vp->effectivePriority = effectivePriority;
    vp->quantum_allowance = QuantumAllowanceForPriority(vp->effectivePriority);
    vp->wait_start_time = MonotonicClock_GetCurrentQuantums();
    
    List_InsertAfterLast(&self->ready_queue.priority[vp->effectivePriority], &vp->rewa_queue_entry);
    
    const int popByteIdx = vp->effectivePriority >> 3;
    const int popBitIdx = vp->effectivePriority - (popByteIdx << 3);
    self->ready_queue.populated[popByteIdx] |= (1 << popBitIdx);
}

// Adds the given virtual processor to the scheduler and makes it eligible for
// running.
void VirtualProcessorScheduler_AddVirtualProcessor(VirtualProcessorScheduler* _Nonnull self, VirtualProcessor* _Nonnull vp)
{
    // Protect against our scheduling code
    const int sps = VirtualProcessorScheduler_DisablePreemption();
    
    VirtualProcessorScheduler_AddVirtualProcessor_Locked(self, vp, vp->priority);
    VirtualProcessorScheduler_RestorePreemption(sps);
}

// Takes the given virtual processor off the ready queue.
void VirtualProcessorScheduler_RemoveVirtualProcessor_Locked(VirtualProcessorScheduler* _Nonnull self, VirtualProcessor* _Nonnull vp)
{
    register const int pri = vp->effectivePriority;
    
    List_Remove(&self->ready_queue.priority[pri], &vp->rewa_queue_entry);
    
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
        
        VirtualProcessor* vp = (VirtualProcessor*)ct->owner;
        VirtualProcessorScheduler_WakeUpOne(self, vp->waiting_on_wait_queue, vp, WAKEUP_REASON_TIMEOUT, false);
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
static void VirtualProcessorScheduler_ArmTimeout(VirtualProcessorScheduler* _Nonnull self, VirtualProcessor* _Nonnull vp, const struct timespec* _Nonnull deadline)
{
    vp->timeout.deadline = Quantums_MakeFromTimespec(deadline, QUANTUM_ROUNDING_AWAY_FROM_ZERO);
    vp->timeout.is_valid = true;
    
    _arm_timeout(self, vp);
}

// Cancels an armed timeout for the given virtual processor. Does nothing if
// no timeout is armed.
static void VirtualProcessorScheduler_CancelTimeout(VirtualProcessorScheduler* _Nonnull self, VirtualProcessor* _Nonnull vp)
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

errno_t VirtualProcessorScheduler_WaitOn(VirtualProcessorScheduler* _Nonnull self, List* _Nonnull waq, int options, const struct timespec* _Nullable wtp, struct timespec* _Nullable rmtp)
{
    VirtualProcessor* vp = (VirtualProcessor*)self->running;
    struct timespec now, deadline;

    assert(vp->sched_state == kVirtualProcessorState_Running);


    // Immediately return instead of waiting if we are in the middle of an abort
    // of a call-as-user invocation.
    if ((vp->flags & (VP_FLAG_CAU_IN_PROGRESS | VP_FLAG_CAU_ABORTED)) == (VP_FLAG_CAU_IN_PROGRESS | VP_FLAG_CAU_ABORTED)) {
        if (rmtp) {
            timespec_clear(rmtp);   // User space won't see this value anyway
        }
        return EINTR;
    }


    // Put us on the timeout queue if a relevant timeout has been specified.
    // Note that we return immediately if we're already past the deadline
    if (wtp) {
        MonotonicClock_GetCurrentTime(&now);

        if ((options & WAIT_ABSTIME) == WAIT_ABSTIME) {
            deadline = *wtp;
        }
        else {
            timespec_add(&now, wtp, &deadline);
        }


        if (timespec_lt(&deadline, &TIMESPEC_INF)) {
            if (timespec_le(&deadline, &now)) {
                if (rmtp) {
                    timespec_clear(rmtp);
                }
                return ETIMEDOUT;
            }

            VirtualProcessorScheduler_ArmTimeout(self, vp, &deadline);
        }
    }


    // Put us on the wait queue. The wait queue is sorted by the QoS and priority
    // from highest to lowest. VPs which enter the queue first, leave it first.
    register VirtualProcessor* pvp = NULL;
    register VirtualProcessor* cvp = (VirtualProcessor*)waq->first;
    while (cvp) {
        if (cvp->effectivePriority < vp->effectivePriority) {
            break;
        }
        
        pvp = cvp;
        cvp = (VirtualProcessor*)cvp->rewa_queue_entry.next;
    }
    
    List_InsertAfter(waq, &vp->rewa_queue_entry, &pvp->rewa_queue_entry);
    
    vp->sched_state = kVirtualProcessorState_Waiting;
    vp->waiting_on_wait_queue = waq;
    vp->wait_start_time = MonotonicClock_GetCurrentQuantums();
    vp->wakeup_reason = WAKEUP_REASON_NONE;

    if ((options & WAIT_INTERRUPTABLE) == WAIT_INTERRUPTABLE) {
        vp->flags |= VP_FLAG_INTERRUPTABLE_WAIT;
    } else {
        vp->flags &= ~VP_FLAG_INTERRUPTABLE_WAIT;
    }

    
    // Find another VP to run and context switch to it
    VirtualProcessorScheduler_SwitchTo(self,
                                       VirtualProcessorScheduler_GetHighestPriorityReady(self));
    
    
    if (rmtp) {
        MonotonicClock_GetCurrentTime(&now);

        if (timespec_lt(&now, &deadline)) {
            timespec_sub(&deadline, &now, rmtp);
        }
        else {
            timespec_clear(rmtp);
        }
    }

    switch (vp->wakeup_reason) {
        case WAKEUP_REASON_INTERRUPTED: return EINTR;
        case WAKEUP_REASON_TIMEOUT:     return ETIMEDOUT;
        default:                        return EOK;
    }
}

// Adds all VPs on the given list to the ready queue. The VPs are removed from
// the wait queue. Expects to be called from an interrupt context and thus defers
// context switches until the return from the interrupt context.
void VirtualProcessorScheduler_WakeUpAllFromInterruptContext(VirtualProcessorScheduler* _Nonnull self, List* _Nonnull waq)
{
    register ListNode* cnp = waq->first;    
    
    // Make all waiting VPs ready to run but do not trigger a context switch.
    while (cnp) {
        register ListNode* nnp = cnp->next;
        
        VirtualProcessorScheduler_WakeUpOne(self, waq, (VirtualProcessor*)cnp, WAKEUP_REASON_FINISHED, false);
        cnp = nnp;
    }
}

// Wakes up, up to 'count' waiters on the wait queue 'waq'. The woken up VPs are
// removed from the wait queue. Expects to be called with preemption disabled.
void VirtualProcessorScheduler_WakeUpSome(VirtualProcessorScheduler* _Nonnull self, List* _Nonnull waq, int count, int wakeUpReason, bool allowContextSwitch)
{
    register ListNode* pCurNode = waq->first;
    register int i = 0;
    VirtualProcessor* pRunCandidate = NULL;
    
    
    // First pass: make all waiting VPs ready and collect as many VPs to run as
    // we got CPU cores on this machine.
    while (pCurNode && i < count) {
        register ListNode* pNextNode = pCurNode->next;
        register VirtualProcessor* vp = (VirtualProcessor*)pCurNode;
        
        VirtualProcessorScheduler_WakeUpOne(self, waq, vp, wakeUpReason, false);
        if (pRunCandidate == NULL && (vp->sched_state == kVirtualProcessorState_Ready && vp->suspension_count == 0)) {
            pRunCandidate = vp;
        }
        pCurNode = pNextNode;
        i++;
    }
    
    
    // Second pass: start running all the VPs that we collected in pass one.
    if (allowContextSwitch && pRunCandidate) {
        VirtualProcessorScheduler_MaybeSwitchTo(self, pRunCandidate);
    }
}

// Adds the given VP from the given wait queue to the ready queue. The VP is
// removed from the wait queue. The scheduler guarantees that a wakeup operation
// will never fail with an error. This doesn't mean that calling this function
// will always result in a virtual processor wakeup. If the wait queue is empty
// then no wakeups will happen. Also a virtual processor that sits in an
// uninterruptible wait or that was suspended while being in a wait state will
// not get woken up.
// May be called from an interrupt context.
void VirtualProcessorScheduler_WakeUpOne(VirtualProcessorScheduler* _Nonnull self, List* _Nonnull waq, VirtualProcessor* _Nonnull vp, int wakeUpReason, bool allowContextSwitch)
{
    assert(waq != NULL);


    // Nothing to do if we are not waiting
    if (vp->sched_state != kVirtualProcessorState_Waiting) {
        return;
    }
    

    // Do not wake up the virtual processor if it is in an uninterruptible wait.
    if (wakeUpReason == WAKEUP_REASON_INTERRUPTED && (vp->flags & VP_FLAG_INTERRUPTABLE_WAIT) == 0) {
        return;
    }


    // Finish the wait. Remove the VP from the wait queue, the timeout queue and
    // store the wake reason.
    List_Remove(waq, &vp->rewa_queue_entry);
    
    VirtualProcessorScheduler_CancelTimeout(self, vp);
    
    vp->waiting_on_wait_queue = NULL;
    vp->wakeup_reason = wakeUpReason;
    vp->flags &= ~VP_FLAG_INTERRUPTABLE_WAIT;
    
    
    if (vp->suspension_count == 0) {
        // Make the VP ready and adjust it's effective priority based on the
        // time it has spent waiting
        const int32_t quatersSlept = (MonotonicClock_GetCurrentQuantums() - vp->wait_start_time) / self->quantums_per_quarter_second;
        const int8_t boostedPriority = __min(vp->effectivePriority + __min(quatersSlept, VP_PRIORITY_HIGHEST), VP_PRIORITY_HIGHEST);
        VirtualProcessorScheduler_AddVirtualProcessor_Locked(self, vp, boostedPriority);
        
        if (allowContextSwitch) {
            VirtualProcessorScheduler_MaybeSwitchTo(self, vp);
        }
    } else {
        // The VP is suspended. Move it to ready state so that it will be
        // added to the ready queue once we resume it.
        vp->sched_state = kVirtualProcessorState_Ready;
    }
}

// Context switches to the given virtual processor if it is a better choice. Eg
// it has a higher priority than the VP that is currently running. This is a
// voluntary (cooperative) context switch which means that it will only happen
// if we are not running in the interrupt context and voluntary context switches
// are enabled.
void VirtualProcessorScheduler_MaybeSwitchTo(VirtualProcessorScheduler* _Nonnull self, VirtualProcessor* _Nonnull vp)
{
    if (vp->sched_state == kVirtualProcessorState_Ready
        && vp->suspension_count == 0
        && VirtualProcessorScheduler_IsCooperationEnabled()) {
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
_Noreturn VirtualProcessorScheduler_TerminateVirtualProcessor(VirtualProcessorScheduler* _Nonnull self, VirtualProcessor* _Nonnull vp)
{
    assert((vp->flags & VP_FLAG_TERMINATED) == VP_FLAG_TERMINATED);
    assert(vp == self->running);
    
    // We don't need to save the old preemption state because this VP is going
    // away and we will never context switch back to it
    (void) VirtualProcessorScheduler_DisablePreemption();
    
    // Put the VP on the finalization queue
    List_InsertAfterLast(&self->finalizer_queue, &vp->rewa_queue_entry);
    
    
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
    
    if (dead_vps_count >= FINALIZE_NOW_THRESHOLD && self->scheduler_wait_queue.first != NULL) {
        // The scheduler VP is currently waiting for work. Let's wake it up.
        VirtualProcessorScheduler_WakeUpOne(self,
                                            &self->scheduler_wait_queue,
                                            self->bootVirtualProcessor,
                                            WAKEUP_REASON_INTERRUPTED,
                                            true);
    } else {
        // Do a forced context switch to whoever is ready
        // NOTE: we do NOT put the currently running VP back on the ready queue
        // because it is dead.
        VirtualProcessorScheduler_SwitchTo(self,
                                           VirtualProcessorScheduler_GetHighestPriorityReady(self));
    }
    
    // NOT REACHED
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
        const int sps = VirtualProcessorScheduler_DisablePreemption();

        // Continue to wait as long as there's nothing to finalize
        while (List_IsEmpty(&self->finalizer_queue)) {
            (void)VirtualProcessorScheduler_WaitOn(self,
                &self->scheduler_wait_queue,
                WAIT_INTERRUPTABLE,
                &timeout,
                NULL);
        }
        
        // Got some work to do. Save off the needed data in local vars and then
        // reenable preemption before we go and do the actual work.
        dead_vps = self->finalizer_queue;
        List_Deinit(&self->finalizer_queue);
        
        VirtualProcessorScheduler_RestorePreemption(sps);
        
        
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
            VirtualProcessor* pNextVP = (VirtualProcessor*)pCurVP->rewa_queue_entry.next;
            
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
            pCurVP = (VirtualProcessor*)pCurVP->rewa_queue_entry.next;
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
