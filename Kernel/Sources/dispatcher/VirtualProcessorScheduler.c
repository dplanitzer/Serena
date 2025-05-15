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
#include <log/Log.h>


extern void VirtualProcessorScheduler_SwitchContext(void);

static void VirtualProcessorScheduler_DumpReadyQueue_Locked(VirtualProcessorScheduler* _Nonnull pScheduler);

static VirtualProcessor* _Nonnull BootVirtualProcessor_Create(BootAllocator* _Nonnull pBootAlloc, VoidFunc_1 _Nonnull pFunc, void* _Nullable _Weak pContext);
static VirtualProcessor* _Nonnull IdleVirtualProcessor_Create(BootAllocator* _Nonnull pBootAlloc);


VirtualProcessorScheduler   gVirtualProcessorSchedulerStorage;
VirtualProcessorScheduler*  gVirtualProcessorScheduler = &gVirtualProcessorSchedulerStorage;


// Initializes the virtual processor scheduler and sets up the boot virtual
// processor plus the idle virtual processor. The 'pFunc' function will be
// invoked in the context of the boot virtual processor and it will receive the
// 'pContext' argument. The first context switch from the machine reset context
// to the boot virtual processor context is triggered by calling the
// VirtualProcessorScheduler_IncipientContextSwitch() function. 
void VirtualProcessorScheduler_CreateForLocalCPU(SystemDescription* _Nonnull pSysDesc, BootAllocator* _Nonnull pBootAlloc, VoidFunc_1 _Nonnull pFunc, void* _Nullable _Weak pContext)
{
    // Stored in the BSS. Thus starts out zeroed.
    VirtualProcessorScheduler* pScheduler = &gVirtualProcessorSchedulerStorage;


    // Initialize the boot virtual processor
    pScheduler->bootVirtualProcessor = BootVirtualProcessor_Create(pBootAlloc, pFunc, pContext);


    // Initialize the idle virtual processor
    pScheduler->idleVirtualProcessor = IdleVirtualProcessor_Create(pBootAlloc);


    // Initialize the scheduler
    if (pSysDesc->fpu_model != FPU_MODEL_NONE) {
        pScheduler->csw_hw |= CSW_HW_HAS_FPU;
    }
    
    List_Init(&pScheduler->timeout_queue);
    List_Init(&pScheduler->sleep_queue);
    List_Init(&pScheduler->scheduler_wait_queue);
    List_Init(&pScheduler->finalizer_queue);

    for (int i = 0; i < VP_PRIORITY_COUNT; i++) {
        List_Init(&pScheduler->ready_queue.priority[i]);
    }
    for (int i = 0; i < VP_PRIORITY_POP_BYTE_COUNT; i++) {
        pScheduler->ready_queue.populated[i] = 0;
    }
    VirtualProcessorScheduler_AddVirtualProcessor_Locked(
        pScheduler,
        pScheduler->bootVirtualProcessor,
        pScheduler->bootVirtualProcessor->priority);
    
    pScheduler->running = NULL;
    pScheduler->scheduled = VirtualProcessorScheduler_GetHighestPriorityReady(pScheduler);
    pScheduler->csw_signals |= CSW_SIGNAL_SWITCH;
    pScheduler->flags |= SCHED_FLAG_VOLUNTARY_CSW_ENABLED;
    VirtualProcessorScheduler_RemoveVirtualProcessor_Locked(pScheduler, pScheduler->scheduled);
    
    assert(pScheduler->scheduled == pScheduler->bootVirtualProcessor);
}

// Called from OnStartup() after the heap has been created. Finishes the scheduler
// initialization.
errno_t VirtualProcessorScheduler_FinishBoot(VirtualProcessorScheduler* _Nonnull pScheduler)
{
    decl_try_err();

    pScheduler->quantums_per_quarter_second = Quantums_MakeFromTimespec(timespec_from_ms(250), QUANTUM_ROUNDING_AWAY_FROM_ZERO);


    // Resume the idle virtual processor
    VirtualProcessor_Resume(pScheduler->idleVirtualProcessor, false);
    

    // Hook us up with the quantum timer interrupt
    InterruptHandlerID irqHandler;
    try(InterruptController_AddDirectInterruptHandler(gInterruptController,
                                                  INTERRUPT_ID_QUANTUM_TIMER,
                                                  INTERRUPT_HANDLER_PRIORITY_HIGHEST - 1,
                                                  (InterruptHandler_Closure)VirtualProcessorScheduler_OnEndOfQuantum,
                                                  pScheduler,
                                                  &irqHandler));
    InterruptController_SetInterruptHandlerEnabled(gInterruptController, irqHandler, true);

    return EOK;

catch:
    return err;
}

// Adds the given virtual processor with the given effective priority to the
// ready queue and resets its time slice length to the length implied by its
// effective priority.
void VirtualProcessorScheduler_AddVirtualProcessor_Locked(VirtualProcessorScheduler* _Nonnull pScheduler, VirtualProcessor* _Nonnull pVP, int effectivePriority)
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
    
    const int popByteIdx = pVP->effectivePriority >> 3;
    const int popBitIdx = pVP->effectivePriority - (popByteIdx << 3);
    pScheduler->ready_queue.populated[popByteIdx] |= (1 << popBitIdx);
}

// Adds the given virtual processor to the scheduler and makes it eligble for
// running.
void VirtualProcessorScheduler_AddVirtualProcessor(VirtualProcessorScheduler* _Nonnull pScheduler, VirtualProcessor* _Nonnull pVP)
{
    // Protect against our scheduling code
    const int sps = VirtualProcessorScheduler_DisablePreemption();
    
    VirtualProcessorScheduler_AddVirtualProcessor_Locked(pScheduler, pVP, pVP->priority);
    VirtualProcessorScheduler_RestorePreemption(sps);
}

// Takes the given virtual processor off the ready queue.
void VirtualProcessorScheduler_RemoveVirtualProcessor_Locked(VirtualProcessorScheduler* _Nonnull pScheduler, VirtualProcessor* _Nonnull pVP)
{
    register const int pri = pVP->effectivePriority;
    
    List_Remove(&pScheduler->ready_queue.priority[pri], &pVP->rewa_queue_entry);
    
    if (List_IsEmpty(&pScheduler->ready_queue.priority[pri])) {
        const int popByteIdx = pri >> 3;
        const int popBitIdx = pri - (popByteIdx << 3);
        pScheduler->ready_queue.populated[popByteIdx] &= ~(1 << popBitIdx);
    }
}

// Find the best VP to run next and return it. Null is returned if no VP is ready
// to run. This will only happen if this function is called from the quantum
// interrupt while the idle VP is the running VP.
VirtualProcessor* _Nullable VirtualProcessorScheduler_GetHighestPriorityReady(VirtualProcessorScheduler* _Nonnull pScheduler)
{
    for (int popByteIdx = VP_PRIORITY_POP_BYTE_COUNT - 1; popByteIdx >= 0; popByteIdx--) {
        const uint8_t popByte = pScheduler->ready_queue.populated[popByteIdx];
        
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
    curRunning->effectivePriority = __max(curRunning->effectivePriority - 1, VP_PRIORITY_LOWEST);
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
static void VirtualProcessorScheduler_ArmTimeout(VirtualProcessorScheduler* _Nonnull pScheduler, VirtualProcessor* pVP, struct timespec deadline)
{
    pVP->timeout.deadline = Quantums_MakeFromTimespec(deadline, QUANTUM_ROUNDING_AWAY_FROM_ZERO);
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
errno_t VirtualProcessorScheduler_WaitOn(VirtualProcessorScheduler* _Nonnull pScheduler, List* _Nonnull pWaitQueue, struct timespec deadline, bool isInterruptable)
{
    VirtualProcessor* pVP = (VirtualProcessor*)pScheduler->running;

    assert(pVP->rewa_queue_entry.next == NULL);
    assert(pVP->rewa_queue_entry.prev == NULL);
    assert(pVP->state != kVirtualProcessorState_Waiting);

    // Immediately return instead of waiting if we are in the middle of an abort
    // of a call-as-user invocation.
    if ((pVP->flags & (VP_FLAG_CAU_IN_PROGRESS | VP_FLAG_CAU_ABORTED)) == (VP_FLAG_CAU_IN_PROGRESS | VP_FLAG_CAU_ABORTED)) {
        return EINTR;
    }


    // Put us on the timeout queue if a relevant timeout has been specified.
    // Note that we return immediately if we're already past the deadline
    if (timespec_ls(deadline, TIMESPEC_INF)) {
        if (timespec_lsq(deadline, MonotonicClock_GetCurrentTime())) {
            return ETIMEDOUT;
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
        case WAKEUP_REASON_TIMEOUT:     return ETIMEDOUT;
        default:                        return EOK;
    }
}

// Adds all VPs on the given list to the ready queue. The VPs are removed from
// the wait queue. Expects to be called from an interrupt context and thus defers
// context switches until the return from the interrupt context.
void VirtualProcessorScheduler_WakeUpAllFromInterruptContext(VirtualProcessorScheduler* _Nonnull pScheduler, List* _Nonnull pWaitQueue)
{
    register ListNode* pCurNode = pWaitQueue->first;    
    
    // Make all waiting VPs ready to run but do not trigger a context switch.
    while (pCurNode) {
        register ListNode* pNextNode = pCurNode->next;
        
        VirtualProcessorScheduler_WakeUpOne(pScheduler, pWaitQueue, (VirtualProcessor*)pCurNode, WAKEUP_REASON_FINISHED, false);
        pCurNode = pNextNode;
    }
}

// Wakes up, up to 'count' waiters on the wait queue 'pWaitQueue'. The woken up
// VPs are removed from the wait queue. Expects to be called with preemption
// disabled.
void VirtualProcessorScheduler_WakeUpSome(VirtualProcessorScheduler* _Nonnull pScheduler, List* _Nonnull pWaitQueue, int count, int wakeUpReason, bool allowContextSwitch)
{
    register ListNode* pCurNode = pWaitQueue->first;
    register int i = 0;
    VirtualProcessor* pRunCandidate = NULL;
    
    
    // First pass: make all waiting VPs ready and collect as many VPs to run as
    // we got CPU cores on this machine.
    while (pCurNode && i < count) {
        register ListNode* pNextNode = pCurNode->next;
        register VirtualProcessor* pVP = (VirtualProcessor*)pCurNode;
        
        VirtualProcessorScheduler_WakeUpOne(pScheduler, pWaitQueue, pVP, wakeUpReason, false);
        if (pRunCandidate == NULL && (pVP->state == kVirtualProcessorState_Ready && pVP->suspension_count == 0)) {
            pRunCandidate = pVP;
        }
        pCurNode = pNextNode;
        i++;
    }
    
    
    // Second pass: start running all the VPs that we collected in pass one.
    if (allowContextSwitch && pRunCandidate) {
        VirtualProcessorScheduler_MaybeSwitchTo(pScheduler, pRunCandidate);
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
void VirtualProcessorScheduler_WakeUpOne(VirtualProcessorScheduler* _Nonnull pScheduler, List* _Nonnull pWaitQueue, VirtualProcessor* _Nonnull pVP, int wakeUpReason, bool allowContextSwitch)
{
    assert(pWaitQueue != NULL);


    // Nothing to do if we are not waiting
    if (pVP->state != kVirtualProcessorState_Waiting) {
        return;
    }
    

    // Do not wake up the virtual processor if it is in an uninterruptible wait.
    if (wakeUpReason == WAKEUP_REASON_INTERRUPTED && (pVP->flags & VP_FLAG_INTERRUPTABLE_WAIT) == 0) {
        return;
    }


    // Finish the wait. Remove the VP from the wait queue, the timeout queue and
    // store the wake reason.
    List_Remove(pWaitQueue, &pVP->rewa_queue_entry);
    
    VirtualProcessorScheduler_CancelTimeout(pScheduler, pVP);
    
    pVP->waiting_on_wait_queue = NULL;
    pVP->wakeup_reason = wakeUpReason;
    pVP->flags &= ~VP_FLAG_INTERRUPTABLE_WAIT;
    
    
    if (pVP->suspension_count == 0) {
        // Make the VP ready and adjust it's effective priority based on the
        // time it has spent waiting
        const int32_t quatersSlept = (MonotonicClock_GetCurrentQuantums() - pVP->wait_start_time) / pScheduler->quantums_per_quarter_second;
        const int8_t boostedPriority = __min(pVP->effectivePriority + __min(quatersSlept, VP_PRIORITY_HIGHEST), VP_PRIORITY_HIGHEST);
        VirtualProcessorScheduler_AddVirtualProcessor_Locked(pScheduler, pVP, boostedPriority);
        
        if (allowContextSwitch) {
            VirtualProcessorScheduler_MaybeSwitchTo(pScheduler, pVP);
        }
    } else {
        // The VP is suspended. Move it to ready state so that it will be
        // added to the ready queue once we resume it.
        pVP->state = kVirtualProcessorState_Ready;
    }
}

// Context switches to the given virtual processor if it is a better choice. Eg
// it has a higher priority than the VP that is currently running. This is a
// voluntary (cooperative) context switch which means that it will only happen
// if we are not running in the interrupt context and voluntary context switches
// are enabled.
void VirtualProcessorScheduler_MaybeSwitchTo(VirtualProcessorScheduler* _Nonnull pScheduler, VirtualProcessor* _Nonnull pVP)
{
    if (pVP->state == kVirtualProcessorState_Ready
        && pVP->suspension_count == 0
        && VirtualProcessorScheduler_IsCooperationEnabled()) {
        VirtualProcessor* pBestReadyVP = VirtualProcessorScheduler_GetHighestPriorityReady(pScheduler);
        
        if (pBestReadyVP == pVP && pVP->effectivePriority >= pScheduler->running->effectivePriority) {
            VirtualProcessor* pCurRunning = (VirtualProcessor*)pScheduler->running;
            
            VirtualProcessorScheduler_AddVirtualProcessor_Locked(pScheduler, pCurRunning, pCurRunning->priority);
            VirtualProcessorScheduler_SwitchTo(pScheduler, pVP);
        }
    }
}

// Context switch to the given virtual processor. The VP must be in ready state
// and on the ready queue. Immediately context switches to the VP.
// Expects that the call has already added the currently running VP to a wait
// queue or the finalizer queue.
void VirtualProcessorScheduler_SwitchTo(VirtualProcessorScheduler* _Nonnull pScheduler, VirtualProcessor* _Nonnull pVP)
{
    VirtualProcessorScheduler_RemoveVirtualProcessor_Locked(pScheduler, pVP);
    pScheduler->scheduled = pVP;
    pScheduler->csw_signals |= CSW_SIGNAL_SWITCH;
    VirtualProcessorScheduler_SwitchContext();
}

// Terminates the given virtual processor that is executing the caller. Does not
// return to the caller. The VP must already have been marked as terminating.
_Noreturn VirtualProcessorScheduler_TerminateVirtualProcessor(VirtualProcessorScheduler* _Nonnull pScheduler, VirtualProcessor* _Nonnull pVP)
{
    assert((pVP->flags & VP_FLAG_TERMINATED) == VP_FLAG_TERMINATED);
    assert(pVP == pScheduler->running);
    
    // We don't need to save the old preemption state because this VP is going
    // away and we will never context switch back to it
    (void) VirtualProcessorScheduler_DisablePreemption();
    
    // Put the VP on the finalization queue
    List_InsertAfterLast(&pScheduler->finalizer_queue, &pVP->rewa_queue_entry);
    
    
    // Check whether there are too many VPs on the finalizer queue. If so then we
    // try to context switch to the scheduler VP otherwise we'll context switch
    // to whoever else is the best candidate to run.
    VirtualProcessor* newRunning;
    const int FINALIZE_NOW_THRESHOLD = 4;
    int dead_vps_count = 0;
    ListNode* pCurNode = pScheduler->finalizer_queue.first;
    while (pCurNode != NULL && dead_vps_count < FINALIZE_NOW_THRESHOLD) {
        pCurNode = pCurNode->next;
        dead_vps_count++;
    }
    
    if (dead_vps_count >= FINALIZE_NOW_THRESHOLD && pScheduler->scheduler_wait_queue.first != NULL) {
        // The scheduler VP is currently waiting for work. Let's wake it up.
        VirtualProcessorScheduler_WakeUpOne(pScheduler,
                                            &pScheduler->scheduler_wait_queue,
                                            pScheduler->bootVirtualProcessor,
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

// Gives the virtual processor scheduler opportunities to run tasks that take
// care of internal duties. This function must be called from the boot virtual
// processor. This function does not return to the caller. 
_Noreturn VirtualProcessorScheduler_Run(VirtualProcessorScheduler* _Nonnull pScheduler)
{
    assert(VirtualProcessor_GetCurrent() == pScheduler->bootVirtualProcessor);
    List dead_vps;

    while (true) {
        List_Init(&dead_vps);
        const int sps = VirtualProcessorScheduler_DisablePreemption();

        // Continue to wait as long as there's nothing to finalize
        while (List_IsEmpty(&pScheduler->finalizer_queue)) {
            (void)VirtualProcessorScheduler_WaitOn(pScheduler, &pScheduler->scheduler_wait_queue,
                                             timespec_add(MonotonicClock_GetCurrentTime(), timespec_from_sec(1)), true);
        }
        
        // Got some work to do. Save off the needed data in local vars and then
        // reenable preemption before we go and do the actual work.
        dead_vps = pScheduler->finalizer_queue;
        List_Deinit(&pScheduler->finalizer_queue);
        
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
static void VirtualProcessorScheduler_DumpReadyQueue_Locked(VirtualProcessorScheduler* _Nonnull pScheduler)
{
    for (int i = 0; i < VP_PRIORITY_COUNT; i++) {
        VirtualProcessor* pCurVP = (VirtualProcessor*)pScheduler->ready_queue.priority[i].first;
        
        while (pCurVP) {
            printf("{pri: %d}, ", pCurVP->priority);
            pCurVP = (VirtualProcessor*)pCurVP->rewa_queue_entry.next;
        }
    }
    printf("\n");
    for (int i = 0; i < VP_PRIORITY_POP_BYTE_COUNT; i++) {
        printf("%hhx, ", pScheduler->ready_queue.populated[i]);
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
static VirtualProcessor* _Nonnull BootVirtualProcessor_Create(BootAllocator* _Nonnull pBootAlloc, VoidFunc_1 _Nonnull pFunc, void* _Nullable _Weak pContext)
{
    // Stored in the BSS. Thus starts out zeroed.
    static VirtualProcessor gBootVirtualProcessorStorage;
    VirtualProcessor* pVP = &gBootVirtualProcessorStorage;


    // Allocate the boot virtual processor kernel stack
    const int kernelStackSize = CPU_PAGE_SIZE;
    char* pKernelStackBase = BootAllocator_Allocate(pBootAlloc, kernelStackSize);


    // Create the VP
    VirtualProcessor_CommonInit(pVP, VP_PRIORITY_HIGHEST);
    try_bang(VirtualProcessor_SetClosure(pVP, VirtualProcessorClosure_MakeWithPreallocatedKernelStack((VoidFunc_1)pFunc, pContext, pKernelStackBase, kernelStackSize)));
    pVP->save_area.sr |= 0x0700;    // IRQs should be disabled by default
    pVP->suspension_count = 0;
    
    return pVP;
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Idle Virtual Processor
////////////////////////////////////////////////////////////////////////////////

static void IdleVirtualProcessor_Run(void* _Nullable pContext);

// Creates an idle virtual processor. The scheduler schedules this VP if no other
// one is in state ready.
static VirtualProcessor* _Nonnull IdleVirtualProcessor_Create(BootAllocator* _Nonnull pBootAlloc)
{
        // Stored in the BSS. Thus starts out zeroed.
    static VirtualProcessor gIdleVirtualProcessorStorage;
    VirtualProcessor* pVP = &gIdleVirtualProcessorStorage;


    // Allocate the boot virtual processor kernel stack
    const int kernelStackSize = CPU_PAGE_SIZE;
    char* pKernelStackBase = BootAllocator_Allocate(pBootAlloc, kernelStackSize);


    // Create the VP
    VirtualProcessor_CommonInit(pVP, VP_PRIORITY_LOWEST);
    try_bang(VirtualProcessor_SetClosure(pVP, VirtualProcessorClosure_MakeWithPreallocatedKernelStack(IdleVirtualProcessor_Run, NULL, pKernelStackBase, kernelStackSize)));

    return pVP;
}

// Puts the CPU to sleep until an interrupt occurs. The interrupt will give the
// scheduler a chance to run some other virtual processor if one is ready.
static void IdleVirtualProcessor_Run(void* _Nullable pContext)
{
    while (true) {
        cpu_sleep(gSystemDescription->cpu_model);
    }
}
