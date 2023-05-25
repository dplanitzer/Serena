//
//  VirtualProcessorScheduler.c
//  Apollo
//
//  Created by Dietmar Planitzer on 2/23/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "VirtualProcessorScheduler.h"
#include "Bytes.h"
#include "MonotonicClock.h"
#include "Platform.h"
#include "InterruptController.h"


extern void VirtualProcessorScheduler_SwitchContext(void);

static void VirtualProcessorScheduler_AddVirtualProcessor_Locked(VirtualProcessorScheduler* _Nonnull pScheduler, VirtualProcessor* _Nonnull pVP, Int effectivePriority);
static void VirtualProcessorScheduler_RemoveVirtualProcessor_Locked(VirtualProcessorScheduler* _Nonnull pScheduler, VirtualProcessor* _Nonnull pVP);
static VirtualProcessor* _Nullable VirtualProcessorScheduler_GetHighestPriorityReady(VirtualProcessorScheduler* _Nonnull pScheduler);
static void VirtualProcessorScheduler_FinishWait(VirtualProcessorScheduler* _Nonnull pScheduler, List* _Nullable pWaitQueue, VirtualProcessor* _Nonnull pVP, Int wakeUpReason);
static void VirtualProcessorScheduler_SwitchTo(VirtualProcessorScheduler* _Nonnull pScheduler, VirtualProcessor* _Nonnull pVP);
static void VirtualProcessorScheduler_DumpReadyQueue_Locked(VirtualProcessorScheduler* _Nonnull pScheduler);

#define QuantumAllowanceForPriority(pri)    ((VP_PRIORITY_HIGHEST - (pri)) >> 3) + 1


void VirtualProcessorScheduler_Init(VirtualProcessorScheduler* _Nonnull pScheduler, SystemDescription* _Nonnull pSysDesc, VirtualProcessor* _Nonnull pBootVP)
{
    Bytes_ClearRange((Byte*)pScheduler, sizeof(VirtualProcessorScheduler));

    if (pSysDesc->fpu_model != FPU_MODEL_NONE) {
        pScheduler->csw_hw |= CSW_HW_HAS_FPU;
    }
    
    List_Init(&pScheduler->timeout_queue);
    List_Init(&pScheduler->sleep_queue);
    List_Init(&pScheduler->scheduler_wait_queue);
    List_Init(&pScheduler->finalizer_queue);

    for (Int i = 0; i < VP_PRIORITY_COUNT; i++) {
        List_Init(&pScheduler->ready_queue.priority[i]);
    }
    for (Int i = 0; i < VP_PRIORITY_POP_BYTE_COUNT; i++) {
        pScheduler->ready_queue.populated[i] = 0;
    }
    VirtualProcessorScheduler_AddVirtualProcessor_Locked(pScheduler, pBootVP, pBootVP->priority);
    
    pScheduler->running = NULL;
    pScheduler->scheduled = VirtualProcessorScheduler_GetHighestPriorityReady(pScheduler);
    pScheduler->csw_signals |= CSW_SIGNAL_SWITCH;
    pScheduler->flags |= SCHED_FLAG_VOLUNTARY_CSW_ENABLED;
    VirtualProcessorScheduler_RemoveVirtualProcessor_Locked(pScheduler, pScheduler->scheduled);
    
    assert(pScheduler->scheduled == pBootVP);
}

// Called from OnStartup() after the heap has been created. Finishes the scheduler
// initialization.
void VirtualProcessorScheduler_FinishBoot(VirtualProcessorScheduler* _Nonnull pScheduler)
{
    pScheduler->quantums_per_quater_second = Quantums_MakeFromTimeInterval(TimeInterval_MakeMilliseconds(250), QUANTUM_ROUNDING_AWAY_FROM_ZERO);
    SchedulerVirtualProcessor_FinishBoot(SchedulerVirtualProcessor_GetShared());
    
    pScheduler->idleVirtualProcessor = IdleVirtualProcessor_Create(SystemDescription_GetShared());
    assert(pScheduler->idleVirtualProcessor != NULL);
    VirtualProcessor_Resume(pScheduler->idleVirtualProcessor, false);
    
    const Int irqHandler = InterruptController_AddDirectInterruptHandler(InterruptController_GetShared(),
                                                  INTERRUPT_ID_QUANTUM_TIMER,
                                                  INTERRUPT_HANDLER_PRIORITY_HIGHEST - 1,
                                                  (InterruptHandler_Closure)VirtualProcessorScheduler_OnEndOfQuantum,
                                                  (Byte*)pScheduler);
    InterruptController_SetInterruptHandlerEnabled(InterruptController_GetShared(), irqHandler, true);
}

// Adds the given virtual processor with the given effective priority to the
// ready queue and resets uts time slice length to the length implied by its
// effective priority.
static void VirtualProcessorScheduler_AddVirtualProcessor_Locked(VirtualProcessorScheduler* _Nonnull pScheduler, VirtualProcessor* _Nonnull pVP, Int effectivePriority)
{
    assert(pVP != NULL);
    assert(pVP->rewa_queue_entry.prev == NULL);
    assert(pVP->rewa_queue_entry.next == NULL);
    assert(pVP->suspension_count == 0);
    
    pVP->state = kVirtualProcessorState_Ready;
    pVP->effectivePriority = effectivePriority;
    pVP->quantum_allowance = QuantumAllowanceForPriority(pVP->effectivePriority);
    pVP->wait_start_time = MonotonicClock_GetCurrentQuantums();
    
    List_InsertAfterLast(&pScheduler->ready_queue.priority[pVP->effectivePriority], &pVP->rewa_queue_entry);
    
    const Int popByteIdx = pVP->effectivePriority >> 3;
    const Int popBitIdx = pVP->effectivePriority - (popByteIdx << 3);
    pScheduler->ready_queue.populated[popByteIdx] |= (1 << popBitIdx);
}

// Adds the given virtual processor to the scheduler and makes it eligble for
// running.
void VirtualProcessorScheduler_AddVirtualProcessor(VirtualProcessorScheduler* _Nonnull pScheduler, VirtualProcessor* _Nonnull pVP)
{
    // Protect against our scheduling code
    const Int sps = VirtualProcessorScheduler_DisablePreemption();
    
    VirtualProcessorScheduler_AddVirtualProcessor_Locked(pScheduler, pVP, pVP->priority);
    VirtualProcessorScheduler_RestorePreemption(sps);
}

// Takes the given virtual processor off the ready queue.
static void VirtualProcessorScheduler_RemoveVirtualProcessor_Locked(VirtualProcessorScheduler* _Nonnull pScheduler, VirtualProcessor* _Nonnull pVP)
{
    register const Int pri = pVP->effectivePriority;
    
    List_Remove(&pScheduler->ready_queue.priority[pri], &pVP->rewa_queue_entry);
    
    if (List_IsEmpty(&pScheduler->ready_queue.priority[pri])) {
        const Int popByteIdx = pri >> 3;
        const Int popBitIdx = pri - (popByteIdx << 3);
        pScheduler->ready_queue.populated[popByteIdx] &= ~(1 << popBitIdx);
    }
}

// Find the best VP to run next and return it. Null is returned if no VP is ready
// to run. This will only happen if this function is called from the quantum
// interrupt while the idle VP is the running VP.
static VirtualProcessor* _Nullable VirtualProcessorScheduler_GetHighestPriorityReady(VirtualProcessorScheduler* _Nonnull pScheduler)
{
    for (Int popByteIdx = VP_PRIORITY_POP_BYTE_COUNT - 1; popByteIdx >= 0; popByteIdx--) {
        const UInt8 popByte = pScheduler->ready_queue.populated[popByteIdx];
        
        if (popByte) {
            if (popByte & 0x80) { return (VirtualProcessor*) pScheduler->ready_queue.priority[(popByteIdx << 3) + 7].first; }
            if (popByte & 0x40) { return (VirtualProcessor*) pScheduler->ready_queue.priority[(popByteIdx << 3) + 6].first; }
            if (popByte & 0x20) { return (VirtualProcessor*) pScheduler->ready_queue.priority[(popByteIdx << 3) + 5].first; }
            if (popByte & 0x10) { return (VirtualProcessor*) pScheduler->ready_queue.priority[(popByteIdx << 3) + 4].first; }
            if (popByte & 0x8)  { return (VirtualProcessor*) pScheduler->ready_queue.priority[(popByteIdx << 3) + 3].first; }
            if (popByte & 0x4)  { return (VirtualProcessor*) pScheduler->ready_queue.priority[(popByteIdx << 3) + 2].first; }
            if (popByte & 0x2)  { return (VirtualProcessor*) pScheduler->ready_queue.priority[(popByteIdx << 3) + 1].first; }
            if (popByte & 0x1)  { return (VirtualProcessor*) pScheduler->ready_queue.priority[(popByteIdx << 3) + 0].first; }
        }
    }
    
    return NULL;
}

// Invoked at the end of every quantum.
void VirtualProcessorScheduler_OnEndOfQuantum(VirtualProcessorScheduler * _Nonnull pScheduler)
{
    // First, go through the timeout queue and move all VPs whose timeouts have
    // expired to the ready queue.
    const Quantums curTime = MonotonicClock_GetCurrentQuantums();
    
    while (pScheduler->timeout_queue.first) {
        register Timeout* pCurTimeout = (Timeout*)pScheduler->timeout_queue.first;
        
        if (pCurTimeout->deadline > curTime) {
            break;
        }
        
        VirtualProcessor* pVP = (VirtualProcessor*)pCurTimeout->owner;
        VirtualProcessorScheduler_WakeUpOne(pScheduler, pVP->waiting_on_wait_queue, pVP, WAKEUP_REASON_TIMEOUT, false);
    }
    
    
    // Second, update the time slice info for the currently running VP
    register VirtualProcessor* curRunning = (VirtualProcessor*)pScheduler->running;
    
    curRunning->quantum_allowance--;
    if (curRunning->quantum_allowance > 0) {
        return;
    }

    
    // The time slice has expired. Lower our priority and then check whether
    // there's another VP on the ready queue which is more important. If so we
    // context switch to that guy. Otherwise we'll continue to run for another
    // time slice.
    curRunning->effectivePriority = max(curRunning->effectivePriority - 1, VP_PRIORITY_LOWEST);
    curRunning->quantum_allowance = QuantumAllowanceForPriority(curRunning->effectivePriority);

    register VirtualProcessor* pBestReady = VirtualProcessorScheduler_GetHighestPriorityReady(pScheduler);
    if (pBestReady == NULL || pBestReady->effectivePriority <= curRunning->effectivePriority) {
        // We didn't find anything better to run. Continue running the currently
        // running VP.
        return;
    }
    
    
    // Move the currently running VP back to the ready queue and pull the new
    // VP off the ready queue
    VirtualProcessorScheduler_RemoveVirtualProcessor_Locked(pScheduler, pBestReady);
    VirtualProcessorScheduler_AddVirtualProcessor_Locked(pScheduler, curRunning, curRunning->priority);

    
    // Request a context switch
    pScheduler->scheduled = pBestReady;
    pScheduler->csw_signals |= CSW_SIGNAL_SWITCH;
}

// Arms a timeout for the given virtual processor. This puts the VP on the timeout
// queue.
static void VirtualProcessorScheduler_ArmTimeout(VirtualProcessorScheduler* _Nonnull pScheduler, VirtualProcessor* pVP, TimeInterval deadline)
{
    pVP->timeout.deadline = Quantums_MakeFromTimeInterval(deadline, QUANTUM_ROUNDING_AWAY_FROM_ZERO);
    pVP->timeout.is_valid = true;
    
    register Timeout* pPrevTimeout = NULL;
    register Timeout* pCurTimeout = (Timeout*)pScheduler->timeout_queue.first;
    while (pCurTimeout) {
        if (pCurTimeout->deadline > pVP->timeout.deadline) {
            break;
        }
        
        pPrevTimeout = pCurTimeout;
        pCurTimeout = (Timeout*)pCurTimeout->queue_entry.next;
    }
    
    List_InsertAfter(&pScheduler->timeout_queue, &pVP->timeout.queue_entry, &pPrevTimeout->queue_entry);
}

// Cancels an armed timeout for the given virtual processor. Does nothing if
// no timeout is armed.
static void VirtualProcessorScheduler_CancelTimeout(VirtualProcessorScheduler* _Nonnull pScheduler, VirtualProcessor* pVP)
{
    if (pVP->timeout.is_valid) {
        List_Remove(&pScheduler->timeout_queue, &pVP->timeout.queue_entry);
        pVP->timeout.deadline = kQuantums_Infinity;
        pVP->timeout.is_valid = false;
    }
}

// Put the currently running VP (the caller) on the given wait queue. Then runs
// the scheduler to select another VP to run and context switches to the new VP
// right away.
// Expects to be called with preemption disabled. Temporarily reenables
// preemption when context switching to another VP. Returns to the caller with
// preemption disabled.
// VPs on the wait queue are ordered by their QoS and effective priority at the
// time when they enter the wait queue. Additionally VPs with the same priority
// are ordered such that the first one to enter the queue is the first one to
// leave the queue.
// Returns a timeout or interrupted error.
ErrorCode VirtualProcessorScheduler_WaitOn(VirtualProcessorScheduler* _Nonnull pScheduler, List* _Nonnull pWaitQueue, TimeInterval deadline, Bool isInterruptable)
{
    VirtualProcessor* pVP = (VirtualProcessor*)pScheduler->running;

    assert(pVP->rewa_queue_entry.next == NULL);
    assert(pVP->rewa_queue_entry.prev == NULL);
    assert(pVP->state != kVirtualProcessorState_Waiting);

    // Put us on the timeout queue if a relevant timeout has been specified.
    // Note that we return immediately if we're already past the deadline
    if (TimeInterval_Less(deadline, kTimeInterval_Infinity)) {
        if (TimeInterval_LessEquals(deadline, MonotonicClock_GetCurrentTime())) {
            return ETIMEOUT;
        }

        VirtualProcessorScheduler_ArmTimeout(pScheduler, pVP, deadline);
    }

    
    // Put us on the wait queue. The wait queue is sorted by the QoS and priority
    // from highest to lowest. VPs which enter the queue first, leave it first.
    register VirtualProcessor* pPrevVP = NULL;
    register VirtualProcessor* pCurVP = (VirtualProcessor*)pWaitQueue->first;
    while (pCurVP) {
        if (pCurVP->effectivePriority < pVP->effectivePriority) {
            break;
        }
        
        pPrevVP = pCurVP;
        pCurVP = (VirtualProcessor*)pCurVP->rewa_queue_entry.next;
    }
    
    List_InsertAfter(pWaitQueue, &pVP->rewa_queue_entry, &pPrevVP->rewa_queue_entry);
    
    pVP->state = kVirtualProcessorState_Waiting;
    pVP->waiting_on_wait_queue = pWaitQueue;
    pVP->wait_start_time = MonotonicClock_GetCurrentQuantums();
    pVP->wakeup_reason = WAKEUP_REASON_NONE;
    if (isInterruptable) {
        pVP->flags |= VP_FLAG_INTERRUPTABLE_WAIT;
    } else {
        pVP->flags &= ~VP_FLAG_INTERRUPTABLE_WAIT;
    }

    
    // Find another VP to run and context switch to it
    VirtualProcessorScheduler_SwitchTo(pScheduler,
                                       VirtualProcessorScheduler_GetHighestPriorityReady(pScheduler));
    
    switch (pVP->wakeup_reason) {
        case WAKEUP_REASON_INTERRUPTED: return EINTR;
        case WAKEUP_REASON_TIMEOUT:     return ETIMEOUT;
        default:                        return EOK;
    }
}

// Context switches to the given virtual processor if it is a better choice. Eg
// it has a higher priority than the VP that is currently running. This is a
// voluntary (cooperative) context switch which means that it will only happen
// if we are not running in the interrupt context and voluntary context switches
// are enabled.
static void VirtualProcessorScheduler_MaybeSwitchTo(VirtualProcessorScheduler* _Nonnull pScheduler, VirtualProcessor* _Nonnull pVP)
{
    if (pVP->state == kVirtualProcessorState_Ready
        && VirtualProcessorScheduler_IsCooperationEnabled()
        && !InterruptController_IsServicingInterrupt(InterruptController_GetShared())) {
        VirtualProcessor* pBestReadyVP = VirtualProcessorScheduler_GetHighestPriorityReady(pScheduler);
        
        if (pBestReadyVP == pVP && pVP->effectivePriority >= pScheduler->running->effectivePriority) {
            VirtualProcessor* pCurRunning = (VirtualProcessor*)pScheduler->running;
            
            VirtualProcessorScheduler_AddVirtualProcessor_Locked(pScheduler, pCurRunning, pCurRunning->priority);
            VirtualProcessorScheduler_SwitchTo(pScheduler, pVP);
        }
    }
}

// Wakes up all waiters on the wait queue 'pWaitQueue'. The woken up VPs are
// removed from the wait queue. Expects to be called with preemption disabled.
ErrorCode VirtualProcessorScheduler_WakeUpAll(VirtualProcessorScheduler* _Nonnull pScheduler, List* _Nonnull pWaitQueue, Bool allowContextSwitch)
{
    return VirtualProcessorScheduler_WakeUpSome(pScheduler, pWaitQueue, INT_MAX, WAKEUP_REASON_FINISHED, allowContextSwitch);
}

// Wakes up up to 'count' waiters on the wait queue 'pWaitQueue'. The woken up
// VPs are removed from the wait queue. Expects to be called with preemption
// disabled.
ErrorCode VirtualProcessorScheduler_WakeUpSome(VirtualProcessorScheduler* _Nonnull pScheduler, List* _Nullable pWaitQueue, Int count, Int wakeUpReason, Bool allowContextSwitch)
{
    register ListNode* pCurNode = pWaitQueue->first;
    register Int i = 0;
    VirtualProcessor* pRunCandidate = NULL;
    ErrorCode err = EOK;
    
    
    // First pass: make all waiting VPs ready and remember the ones we might
    // want to run.
    while (pCurNode && i < count) {
        register ListNode* pNextNode = pCurNode->next;
        register VirtualProcessor* pVP = (VirtualProcessor*)pCurNode;
        
        const ErrorCode err1 = VirtualProcessorScheduler_WakeUpOne(pScheduler, pWaitQueue, pVP, wakeUpReason, false);
        if (err == EOK && err1 != EOK) {
            err = err1;
        }
        if (pRunCandidate == NULL && ((VirtualProcessor*)pCurNode)->state == kVirtualProcessorState_Ready) {
            pRunCandidate = (VirtualProcessor*)pCurNode;
        }
        pCurNode = pNextNode;
        i++;
    }
    
    
    // Second pass: context switch to the VPs we want to run if they are of a
    // higher priority than what is currently running
    if (allowContextSwitch && pRunCandidate) {
        VirtualProcessorScheduler_MaybeSwitchTo(pScheduler, pRunCandidate);
    }
    
    return err;
}

// Wakes up a specific VP waiting on the wait queue 'pWaitQueue'. The woken up
// VP is removed from the wait queue. Expects to be called with preemption
// disabled.
// Returns EBUSY if the wakeup reason is interrupt but the VP is in an uninterruptable
// sleep.
ErrorCode VirtualProcessorScheduler_WakeUpOne(VirtualProcessorScheduler* _Nonnull pScheduler, List* _Nullable pWaitQueue, VirtualProcessor* _Nonnull pVP, Int wakeUpReason, Bool allowContextSwitch)
{
    // It's possible that the VP that we want to wake up is running if the wakeup is triggered by an interrupt routine.
    // That's okay in this case and we simply return. It's the resonsibility of the interrupt handler to ensure that the
    // fact that it wanted to wake up the VP is noted somehwere. Eg by using a semaphore.
    if (InterruptController_IsServicingInterrupt(InterruptController_GetShared())) {
        if (pScheduler->running == pVP) {
            return EOK;
        }
    }
    
    if (wakeUpReason == WAKEUP_REASON_INTERRUPTED && (pVP->flags & VP_FLAG_INTERRUPTABLE_WAIT) == 0) {
        return EBUSY;
    }
    
    VirtualProcessorScheduler_FinishWait(pScheduler, pWaitQueue, pVP, wakeUpReason);
    
    
    // Everything below this point only applies if the VP that we want to wake up
    // is not currently suspened.
    if (pVP->state == kVirtualProcessorState_Waiting) {
        // Make the VP ready and adjust it's effective priority based on the time it
        // has spent waiting
        const Int32 quatersSlept = (MonotonicClock_GetCurrentQuantums() - pVP->wait_start_time) / pScheduler->quantums_per_quater_second;
        const Int8 boostedPriority = min(pVP->effectivePriority + min(quatersSlept, VP_PRIORITY_HIGHEST), VP_PRIORITY_HIGHEST);
        VirtualProcessorScheduler_AddVirtualProcessor_Locked(pScheduler, pVP, boostedPriority);
        
        if (allowContextSwitch) {
            VirtualProcessorScheduler_MaybeSwitchTo(pScheduler, pVP);
        }
    }
    
    return EOK;
}

// Finishes a wait operation. Expects to becalled with preemption disabled and
// that the given VP is waiting on a wait queue or a timeout. Removes the VP from
// the wait queue, the timeout queue and stores the wake reason. However this
// function does not trigger scheduling or context switching.
static void VirtualProcessorScheduler_FinishWait(VirtualProcessorScheduler* _Nonnull pScheduler, List* _Nullable pWaitQueue, VirtualProcessor* _Nonnull pVP, Int wakeUpReason)
{
    assert(pScheduler->running != pVP);
    
    if (pWaitQueue) {
        List_Remove(pWaitQueue, &pVP->rewa_queue_entry);
    }
    
    VirtualProcessorScheduler_CancelTimeout(pScheduler, pVP);
    
    pVP->waiting_on_wait_queue = NULL;
    pVP->wakeup_reason = wakeUpReason;
    pVP->flags &= ~VP_FLAG_INTERRUPTABLE_WAIT;
}

// Context switch to the given virtual processor. The VP must be in ready state
// and on the ready queue. Immediately context switches to the VP.
// Expects that the call has already added the currently running VP to a wait
// queue or the finalizer queue.
static void VirtualProcessorScheduler_SwitchTo(VirtualProcessorScheduler* _Nonnull pScheduler, VirtualProcessor* _Nonnull pVP)
{
    VirtualProcessorScheduler_RemoveVirtualProcessor_Locked(pScheduler, pVP);
    pScheduler->scheduled = pVP;
    pScheduler->csw_signals |= CSW_SIGNAL_SWITCH;
    VirtualProcessorScheduler_SwitchContext();
}

static void VirtualProcessorScheduler_DumpReadyQueue_Locked(VirtualProcessorScheduler* _Nonnull pScheduler)
{
    for (Int i = 0; i < VP_PRIORITY_COUNT; i++) {
        VirtualProcessor* pCurVP = (VirtualProcessor*)pScheduler->ready_queue.priority[i].first;
        
        while (pCurVP) {
            print("{pri: %d}, ", pCurVP->priority);
            pCurVP = (VirtualProcessor*)pCurVP->rewa_queue_entry.next;
        }
    }
    print("\n");
    for (Int i = 0; i < VP_PRIORITY_POP_BYTE_COUNT; i++) {
        print("%hhx, ", pScheduler->ready_queue.populated[i]);
    }
    print("\n");
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
////////////////////////////////////////////////////////////////////////////////


// Schedules the calling virtual processor for finalization. Does not return.
void VirtualProcessor_ScheduleFinalization(VirtualProcessor* _Nonnull pVP)
{
    VirtualProcessorScheduler* pScheduler = VirtualProcessorScheduler_GetShared();
    assert(pVP == pScheduler->running);
    
    // We don't need to save the old preemption state because this VP is going away
    // and we will never contrext switch back to it
    (void) VirtualProcessorScheduler_DisablePreemption();
    
    // Put the VP on the finalization queue
    List_InsertAfterLast(&pScheduler->finalizer_queue, &pVP->rewa_queue_entry);
    
    
    // Check whether there are too many VPs on the finalizer queue. If so then we
    // try to context switch to the scheduler VP otherwise we'll context switch to
    // whoever else is the bestz camdidate to run.
    VirtualProcessor* newRunning;
    const Int FINALIZE_NOW_THRESHOLD = 4;
    Int dead_vps_count = 0;
    ListNode* pCurNode = pScheduler->finalizer_queue.first;
    while (pCurNode != NULL && dead_vps_count < FINALIZE_NOW_THRESHOLD) {
        pCurNode = pCurNode->next;
        dead_vps_count++;
    }
    
    if (dead_vps_count >= FINALIZE_NOW_THRESHOLD && pScheduler->scheduler_wait_queue.first != NULL) {
        // The scheduler VP is currently waiting for work. Let's wake it up.
        VirtualProcessorScheduler_WakeUpOne(pScheduler,
                                            &pScheduler->scheduler_wait_queue,
                                            SchedulerVirtualProcessor_GetShared(),
                                            WAKEUP_REASON_INTERRUPTED,
                                            true);
    } else {
        // Do a forced context switch to whoever is ready
        // NOTE: we do NOT put the currently running VP back on the ready queue
        // because it is dead.
        VirtualProcessorScheduler_SwitchTo(pScheduler,
                                           VirtualProcessorScheduler_GetHighestPriorityReady(pScheduler));
    }
    
    // NOT REACHED
}

// Sleep for the given number of seconds.
ErrorCode VirtualProcessor_Sleep(TimeInterval delay)
{
    VirtualProcessorScheduler* pScheduler = VirtualProcessorScheduler_GetShared();
    const TimeInterval curTime = MonotonicClock_GetCurrentTime();
    const TimeInterval deadline = TimeInterval_Add(curTime, delay);

    
    // Use the DelayUntil() facility for short waits and context switching for medium and long waits
    if (MonotonicClock_DelayUntil(deadline)) {
        return EOK;
    }
    
    
    // This is a medium or long wait -> context switch away
    Int sps = VirtualProcessorScheduler_DisablePreemption();
    const Int err = VirtualProcessorScheduler_WaitOn(pScheduler, &pScheduler->sleep_queue, deadline, true);
    VirtualProcessorScheduler_RestorePreemption(sps);
    
    return (err == EINTR) ? EINTR : EOK;
}

// Returns the priority of the given VP.
Int VirtualProcessor_GetPriority(VirtualProcessor* _Nonnull pVP)
{
    const Int sps = VirtualProcessorScheduler_DisablePreemption();
    const Int pri = pVP->priority;
    
    VirtualProcessorScheduler_RestorePreemption(sps);
    return pri;
}

// Changes the priority of a virtual processor. Does not immediately reschedule
// the VP if it is currently running. Instead the VP is allowed to finish its
// current quanta.
// XXX might want to change that in the future?
void VirtualProcessor_SetPriority(VirtualProcessor* _Nonnull pVP, Int priority)
{
    VirtualProcessorScheduler* pScheduler = VirtualProcessorScheduler_GetShared();
    Int sps = VirtualProcessorScheduler_DisablePreemption();
    
    if (pVP->priority != priority) {
        switch (pVP->state) {
            case kVirtualProcessorState_Ready: {
                VirtualProcessorScheduler_RemoveVirtualProcessor_Locked(pScheduler, pVP);
                pVP->priority = priority;
                VirtualProcessorScheduler_AddVirtualProcessor_Locked(pScheduler, pVP, pVP->priority);
                break;
            }
                
            case kVirtualProcessorState_Waiting:
            case kVirtualProcessorState_Suspended:
                pVP->priority = priority;
                break;
                
            case kVirtualProcessorState_Running:
                pVP->priority = priority;
                pVP->effectivePriority = priority;
                pVP->quantum_allowance = QuantumAllowanceForPriority(pVP->effectivePriority);
                break;
        }
    }
    VirtualProcessorScheduler_RestorePreemption(sps);
}

// Returns true if the given virtual processor is currently suspended; false otherwise.
Bool VirtualProcessor_IsSuspended(VirtualProcessor* _Nonnull pVP)
{
    Bool isSuspended;
    const Int sps = VirtualProcessorScheduler_DisablePreemption();
    
    isSuspended = pVP->state == kVirtualProcessorState_Suspended;
    VirtualProcessorScheduler_RestorePreemption(sps);
    return isSuspended;
}

// Suspends the calling virtual processor. This function supports nested calls.
ErrorCode VirtualProcessor_Suspend(VirtualProcessor* _Nonnull pVP)
{
    VirtualProcessorScheduler* pScheduler = VirtualProcessorScheduler_GetShared();
    const Int sps = VirtualProcessorScheduler_DisablePreemption();
    
    if (pVP->suspension_count == INT8_MAX) {
        VirtualProcessorScheduler_RestorePreemption(sps);
        return EPARAM;
    }
    
    const Int8 oldState = pVP->state;
    pVP->suspension_count++;
    pVP->state = kVirtualProcessorState_Suspended;
    
    switch (oldState) {
        case kVirtualProcessorState_Ready:
            VirtualProcessorScheduler_RemoveVirtualProcessor_Locked(pScheduler, pVP);
            break;
            
        case kVirtualProcessorState_Running:
            VirtualProcessorScheduler_SwitchTo(pScheduler,
                                               VirtualProcessorScheduler_GetHighestPriorityReady(pScheduler));
            break;
            
        case kVirtualProcessorState_Waiting:
            VirtualProcessorScheduler_WakeUpOne(pScheduler, pVP->waiting_on_wait_queue, pVP, WAKEUP_REASON_INTERRUPTED, false);
            break;
            
        case kVirtualProcessorState_Suspended:
            // We are already suspended
            break;
            
        default:
            abort();
            
    }
    
    VirtualProcessorScheduler_RestorePreemption(sps);
    return EOK;
}

// Resumes the given virtual processor. The virtual processor is forcefully
// resumed if 'force' is true. This means that it is resumed even if the suspension
// count is > 1.
ErrorCode VirtualProcessor_Resume(VirtualProcessor* _Nonnull pVP, Bool force)
{
    VirtualProcessorScheduler* pScheduler = VirtualProcessorScheduler_GetShared();
    const Int sps = VirtualProcessorScheduler_DisablePreemption();
    
    if (pVP->suspension_count == 0) {
        VirtualProcessorScheduler_RestorePreemption(sps);
        return EPARAM;
    }
    
    pVP->suspension_count = force ? 0 : pVP->suspension_count - 1;
    
    if (pVP->suspension_count == 0) {
        VirtualProcessorScheduler_AddVirtualProcessor_Locked(pScheduler, pVP, pVP->priority);
        VirtualProcessorScheduler_MaybeSwitchTo(pScheduler, pVP);
    }
    
    VirtualProcessorScheduler_RestorePreemption(sps);
    return EOK;
}
