//
//  DispatchQueue.c
//  kernel
//
//  Created by Dietmar Planitzer on 4/21/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "DispatchQueuePriv.h"

class_func_defs(DispatchQueue, Object,
override_func_def(deinit, DispatchQueue, Object)
);


DispatchQueueRef    gMainDispatchQueue;


errno_t DispatchQueue_Create(int minConcurrency, int maxConcurrency, int qos, int priority, VirtualProcessorPoolRef _Nonnull vpPoolRef, ProcessRef _Nullable _Weak pProc, DispatchQueueRef _Nullable * _Nonnull pOutQueue)
{
    decl_try_err();
    DispatchQueueRef self = NULL;

    if (maxConcurrency < 1 || maxConcurrency > INT8_MAX) {
        throw(EINVAL);
    }
    if (minConcurrency < 0 || minConcurrency > maxConcurrency) {
        throw(EINVAL);
    }
    
    try(Object_CreateWithExtraBytes(DispatchQueue, sizeof(ConcurrencyLane) * (maxConcurrency - 1), &self));
    SList_Init(&self->item_queue);
    SList_Init(&self->timer_queue);
    SList_Init(&self->item_cache_queue);
    SList_Init(&self->timer_cache_queue);
    SList_Init(&self->completion_signaler_cache_queue);
    Lock_Init(&self->lock);
    ConditionVariable_Init(&self->work_available_signaler);
    ConditionVariable_Init(&self->vp_shutdown_signaler);
    self->owning_process = pProc;
    self->descriptor = -1;
    self->virtual_processor_pool = vpPoolRef;
    self->state = kQueueState_Running;
    self->minConcurrency = (int8_t)minConcurrency;
    self->maxConcurrency = (int8_t)maxConcurrency;
    self->qos = qos;
    self->priority = priority;

    for (int i = 0; i < minConcurrency; i++) {
        try(DispatchQueue_AcquireVirtualProcessor_Locked(self));
    }

    *pOutQueue = self;
    return EOK;

catch:
    Object_Release(self);
    *pOutQueue = NULL;
    return err;
}

// Removes all queued work items, one-shot and repeatable timers from the queue.
static void DispatchQueue_Flush_Locked(DispatchQueueRef _Nonnull self)
{
    // Flush the work item queue
    WorkItemRef pItem;
    while ((pItem = (WorkItemRef) SList_RemoveFirst(&self->item_queue)) != NULL) {
        WorkItem_SignalCompletion(pItem, true);
        DispatchQueue_RelinquishWorkItem_Locked(self, pItem);
    }


    // Flush the timers
    TimerRef pTimer;
    while ((pTimer = (TimerRef) SList_RemoveFirst(&self->timer_queue)) != NULL) {
        DispatchQueue_RelinquishTimer_Locked(self, pTimer);
    }
}

// Terminates the dispatch queue. This does:
// *) an abort of ongoing call-as-user operations on all VPs attached to the queue
// *) flushes the queue
// *) stops the queue from accepting new work
// *) informs the attached process that the queue has terminated
// *) Marks the queue as terminated
// This function initiates the termination of the given dispatch queue. The
// termination process is asynchronous and does not block the caller. It only
// returns once the queue is in terminated state. Note that there is no guarantee
// whether a particular work item that is queued before this function is called
// will still execute or not. However there is a guarantee that once this function
// returns, that no further work items will execute.
void DispatchQueue_Terminate(DispatchQueueRef _Nonnull self)
{
    // Request queue termination. This will stop all dispatch calls from
    // accepting new work items and repeatable timers from rescheduling. This
    // will also cause the VPs to exit their work loop and to relinquish
    // themselves.
    Lock_Lock(&self->lock);
    if (self->state >= kQueueState_Terminating) {
        Lock_Unlock(&self->lock);
        return;
    }
    self->state = kQueueState_Terminating;


    // Flush the dispatch queue which means that we get rid of all still queued
    // work items and timers.
    DispatchQueue_Flush_Locked(self);


    // Abort all ongoing call-as-user invocations.
    for (int i = 0; i < self->maxConcurrency; i++) {
        if (self->concurrency_lanes[i].vp != NULL) {
            VirtualProcessor_AbortCallAsUser(self->concurrency_lanes[i].vp);
        }
    }


    // We want to wake _all_ VPs up here since all of them need to relinquish
    // themselves.
    ConditionVariable_BroadcastAndUnlock(&self->work_available_signaler, &self->lock);
}

// Waits until the dispatch queue has reached 'terminated' state which means that
// all VPs have been relinquished.
void DispatchQueue_WaitForTerminationCompleted(DispatchQueueRef _Nonnull self)
{
    Lock_Lock(&self->lock);
    while (self->availableConcurrency > 0) {
        ConditionVariable_Wait(&self->vp_shutdown_signaler, &self->lock, kTimeInterval_Infinity);
    }


    // The queue is now in terminated state
    self->state = kQueueState_Terminated;
    Lock_Unlock(&self->lock);
}

// Deallocates the dispatch queue. Expects that the queue is in 'terminated' state.
static void _DispatchQueue_Destroy(DispatchQueueRef _Nonnull self)
{
    CompletionSignaler* pCompSignaler;
    WorkItemRef pItem;
    TimerRef pTimer;

    assert(self->state == kQueueState_Terminated);

    // No more VPs are attached to this queue. We can now go ahead and free
    // all resources.
    SList_Deinit(&self->item_queue);      // guaranteed to be empty at this point
    SList_Deinit(&self->timer_queue);     // guaranteed to be empty at this point

    while ((pItem = (WorkItemRef) SList_RemoveFirst(&self->item_cache_queue)) != NULL) {
        WorkItem_Destroy(pItem);
    }
    SList_Deinit(&self->item_cache_queue);
        
    while ((pTimer = (TimerRef) SList_RemoveFirst(&self->timer_cache_queue)) != NULL) {
        Timer_Destroy(pTimer);
    }
    SList_Deinit(&self->timer_cache_queue);

    while((pCompSignaler = (CompletionSignaler*) SList_RemoveFirst(&self->completion_signaler_cache_queue)) != NULL) {
        CompletionSignaler_Destroy(pCompSignaler);
    }
    SList_Deinit(&self->completion_signaler_cache_queue);
        
    Lock_Deinit(&self->lock);
    ConditionVariable_Deinit(&self->work_available_signaler);
    ConditionVariable_Deinit(&self->vp_shutdown_signaler);
    self->owning_process = NULL;
    self->virtual_processor_pool = NULL;
}

// Destroys the dispatch queue. The queue is first terminated if it isn't already
// in terminated state. All work items and timers which are still queued up are
// flushed and will not execute anymore. Blocks the caller until the queue has
// been drained, terminated and deallocated.
void DispatchQueue_deinit(DispatchQueueRef _Nonnull self)
{
    DispatchQueue_Terminate(self);
    DispatchQueue_WaitForTerminationCompleted(self);
    _DispatchQueue_Destroy(self);
}

// Makes sure that we have enough virtual processors attached to the dispatch queue
// and acquires a virtual processor from the virtual processor pool if necessary.
// The virtual processor is attached to the dispatch queue and remains attached
// until it is relinquished by the queue.
static errno_t DispatchQueue_AcquireVirtualProcessor_Locked(DispatchQueueRef _Nonnull self)
{
    decl_try_err();

    // Acquire a new virtual processor if we haven't already filled up all
    // concurrency lanes available to us and one of the following is true:
    // - we don't own any virtual processor at all
    // - we have < minConcurrency virtual processors (remember that this can be 0)
    // - we've queued up at least 4 work items and < maxConcurrency virtual processors
    if (self->state == kQueueState_Running
        && (self->availableConcurrency == 0
            || self->availableConcurrency < self->minConcurrency
            || (self->items_queued_count > 4 && self->availableConcurrency < self->maxConcurrency))) {
        int conLaneIdx = -1;

        for (int i = 0; i < self->maxConcurrency; i++) {
            if (self->concurrency_lanes[i].vp == NULL) {
                conLaneIdx = i;
                break;
            }
        }
        assert(conLaneIdx != -1);

        const int priority = self->qos * kDispatchPriority_Count + (self->priority + kDispatchPriority_Count / 2) + VP_PRIORITIES_RESERVED_LOW;
        VirtualProcessor* pVP = NULL;
        try(VirtualProcessorPool_AcquireVirtualProcessor(
                                            self->virtual_processor_pool,
                                            VirtualProcessorParameters_Make((Closure1Arg_Func)DispatchQueue_Run, self, VP_DEFAULT_KERNEL_STACK_SIZE, VP_DEFAULT_USER_STACK_SIZE, priority),
                                            &pVP));

        VirtualProcessor_SetDispatchQueue(pVP, self, conLaneIdx);
        self->concurrency_lanes[conLaneIdx].vp = pVP;
        self->availableConcurrency++;

        VirtualProcessor_Resume(pVP, false);
    }

    return EOK;

catch:
    return err;
}

// Relinquishes the given virtual processor. The associated concurrency lane is
// freed up and the virtual processor is returned to the virtual processor pool
// after it has been detached from the dispatch queue. This method should only
// be called right before returning from the Dispatch_Run() method which is the
// method that runs on the virtual processor to execute work items.
static void DispatchQueue_RelinquishVirtualProcessor_Locked(DispatchQueueRef _Nonnull self, VirtualProcessor* _Nonnull pVP)
{
    int conLaneIdx = pVP->dispatchQueueConcurrencyLaneIndex;

    assert(conLaneIdx >= 0 && conLaneIdx < self->maxConcurrency);

    VirtualProcessor_SetDispatchQueue(pVP, NULL, -1);
    self->concurrency_lanes[conLaneIdx].vp = NULL;
    self->availableConcurrency--;
}

// Creates a work item for the given closure and closure context. Tries to reuse
// an existing work item from the work item cache whenever possible. Expects that
// the caller holds the dispatch queue lock.
static errno_t DispatchQueue_AcquireWorkItem_Locked(DispatchQueueRef _Nonnull self, DispatchQueueClosure closure, WorkItemRef _Nullable * _Nonnull pOutItem)
{
    decl_try_err();
    WorkItemRef pItem = (WorkItemRef) SList_RemoveFirst(&self->item_cache_queue);

    if (pItem != NULL) {
        WorkItem_Init(pItem, kItemType_Immediate, closure, true);
        self->item_cache_count--;
        *pOutItem = pItem;
    } else {
        try(WorkItem_Create_Internal(closure, true, pOutItem));
    }
    return EOK;

catch:
    *pOutItem = NULL;
    return err;
}

// Relinquishes the given work item. A work item owned by the dispatch queue is
// moved back to the item reuse cache if possible or freed if the cache is full.
// Does nothing if the dispatch queue does not own the item.
static void DispatchQueue_RelinquishWorkItem_Locked(DispatchQueueRef _Nonnull self, WorkItemRef _Nonnull pItem)
{
    if (!pItem->is_owned_by_queue) {
        return;
    }

    if (self->item_cache_count < MAX_ITEM_CACHE_COUNT) {
        WorkItem_Deinit(pItem);
        SList_InsertBeforeFirst(&self->item_cache_queue, &pItem->queue_entry);
        self->item_cache_count++;
    } else {
        WorkItem_Destroy(pItem);
    }
}

// Creates a timer for the given closure and closure context. Tries to reuse
// an existing timer from the timer cache whenever possible. Expects that the
// caller holds the dispatch queue lock.
static errno_t DispatchQueue_AcquireTimer_Locked(DispatchQueueRef _Nonnull self, TimeInterval deadline, TimeInterval interval, DispatchQueueClosure closure, TimerRef _Nullable * _Nonnull pOutTimer)
{
    decl_try_err();
    TimerRef pTimer = (TimerRef) SList_RemoveFirst(&self->timer_cache_queue);

    if (pTimer != NULL) {
        Timer_Init(pTimer, deadline, interval, closure, true);
        self->timer_cache_count--;
        *pOutTimer = pTimer;
    } else {
        try(Timer_Create_Internal(deadline, interval, closure, true, pOutTimer));
    }
    return EOK;

catch:
    *pOutTimer = NULL;
    return err;
}

// Relinquishes the given timer. A timer owned by the queue is moved back to the
// timer reuse queue if possible or freed id the reuse cache is already full.
// Does nothing if the queue does not own the timer.
static void DispatchQueue_RelinquishTimer_Locked(DispatchQueueRef _Nonnull self, TimerRef _Nonnull pTimer)
{
    if (!pTimer->item.is_owned_by_queue) {
        return;
    }

    if (self->timer_cache_count < MAX_TIMER_CACHE_COUNT) {
        Timer_Deinit(pTimer);
        SList_InsertBeforeFirst(&self->timer_cache_queue, &pTimer->item.queue_entry);
        self->timer_cache_count++;
    } else {
        Timer_Destroy(pTimer);
    }
}

// Creates a completion signaler. Tries to reuse an existing completion signaler
// from the completion signaler cache whenever possible. Expects that
// the caller holds the dispatch queue lock.
static errno_t DispatchQueue_AcquireCompletionSignaler_Locked(DispatchQueueRef _Nonnull self, CompletionSignaler* _Nullable * _Nonnull pOutComp)
{
    decl_try_err();
    CompletionSignaler* pItem = (CompletionSignaler*) SList_RemoveFirst(&self->completion_signaler_cache_queue);

    if (pItem != NULL) {
        CompletionSignaler_Init(pItem);
        self->completion_signaler_count--;
        *pOutComp = pItem;
    } else {
        try(CompletionSignaler_Create(pOutComp));
    }
    return EOK;

catch:
    *pOutComp = NULL;
    return err;
}

// Relinquishes the given completion signaler back to the completion signaler
// cache if possible. The completion signaler is freed if the cache is at capacity.
static void DispatchQueue_RelinquishCompletionSignaler_Locked(DispatchQueueRef _Nonnull self, CompletionSignaler* _Nonnull pItem)
{
    if (self->completion_signaler_count < MAX_COMPLETION_SIGNALER_CACHE_COUNT) {
        CompletionSignaler_Deinit(pItem);
        SList_InsertBeforeFirst(&self->completion_signaler_cache_queue, &pItem->queue_entry);
        self->completion_signaler_count++;
    } else {
        CompletionSignaler_Destroy(pItem);
    }
}

// Asynchronously executes the given work item. The work item is executed as
// soon as possible. Expects to be called with the dispatch queue held. Returns
// with the dispatch queue unlocked.
static errno_t DispatchQueue_DispatchWorkItemAsyncAndUnlock_Locked(DispatchQueueRef _Nonnull self, WorkItemRef _Nonnull pItem)
{
    decl_try_err();

    SList_InsertAfterLast(&self->item_queue, &pItem->queue_entry);
    self->items_queued_count++;

    try(DispatchQueue_AcquireVirtualProcessor_Locked(self));
    ConditionVariable_SignalAndUnlock(&self->work_available_signaler, &self->lock);
    
    return EOK;

catch:
    return err;
}

// Synchronously executes the given work item. The work item is executed as
// soon as possible and the caller remains blocked until the work item has finished
// execution. Expects that the caller holds the dispatch queue lock. Returns with
// the dispatch queue unlocked.
static errno_t DispatchQueue_DispatchWorkItemSyncAndUnlock_Locked(DispatchQueueRef _Nonnull self, WorkItemRef _Nonnull pItem)
{
    decl_try_err();
    CompletionSignaler* pCompSignaler = NULL;
    bool isLocked = true;
    bool wasInterrupted = false;

    try(DispatchQueue_AcquireCompletionSignaler_Locked(self, &pCompSignaler));

    // The work item maintains a weak reference to the cached completion semaphore
    pItem->completion = pCompSignaler;

    try(DispatchQueue_DispatchWorkItemAsyncAndUnlock_Locked(self, pItem));
    isLocked = false;
    // Queue is now unlocked
    try(Semaphore_Acquire(&pCompSignaler->semaphore, kTimeInterval_Infinity));

    Lock_Lock(&self->lock);
    isLocked = true;

    if (self->state >= kQueueState_Terminating) {
        // We want to return EINTR if the DispatchSync was interrupted by a
        // DispatchQueue_Terminate()
        wasInterrupted = true;
    } else {
        wasInterrupted = pCompSignaler->isInterrupted;
    }

    DispatchQueue_RelinquishCompletionSignaler_Locked(self, pCompSignaler);
    Lock_Unlock(&self->lock);

    return (wasInterrupted) ? EINTR : EOK;

catch:
    if (pCompSignaler) {
        if (!isLocked) {
            Lock_Lock(&self->lock);
        }
        DispatchQueue_RelinquishCompletionSignaler_Locked(self, pCompSignaler);
        if (!isLocked) {
            Lock_Unlock(&self->lock);
        }
    }
    return err;
}

// Removes all scheduled instances of the given work item from the dispatch
// queue.
static void DispatchQueue_RemoveWorkItem_Locked(DispatchQueueRef _Nonnull self, WorkItemRef _Nonnull pItem)
{
    WorkItemRef pCurItem = (WorkItemRef) self->item_queue.first;
    WorkItemRef pPrevItem = NULL;

    while (pCurItem) {
        if (pCurItem == pItem) {
            WorkItemRef pNextItem = (WorkItemRef) pCurItem->queue_entry.next;

            WorkItem_SignalCompletion(pCurItem, true);
            SList_Remove(&self->item_queue, &pPrevItem->queue_entry, &pCurItem->queue_entry);
            self->items_queued_count--;
            DispatchQueue_RelinquishWorkItem_Locked(self, pCurItem);
            // pPrevItem doesn't change here
            pCurItem = pNextItem;
        }
        else {
            pPrevItem = pCurItem;
            pCurItem = (WorkItemRef) pCurItem->queue_entry.next;
        }
    }
}

// Adds the given timer to the timer queue. Expects that the queue is already
// locked. Does not wake up the queue.
static void DispatchQueue_AddTimer_Locked(DispatchQueueRef _Nonnull self, TimerRef _Nonnull pTimer)
{
    TimerRef pPrevTimer = NULL;
    TimerRef pCurTimer = (TimerRef)self->timer_queue.first;
    
    while (pCurTimer) {
        if (TimeInterval_Greater(pCurTimer->deadline, pTimer->deadline)) {
            break;
        }
        
        pPrevTimer = pCurTimer;
        pCurTimer = (TimerRef)pCurTimer->item.queue_entry.next;
    }
    
    SList_InsertAfter(&self->timer_queue, &pTimer->item.queue_entry, &pPrevTimer->item.queue_entry);
}

// Asynchronously executes the given timer when it comes due. Expects that the
// caller holds the dispatch queue lock.
errno_t DispatchQueue_DispatchTimer_Locked(DispatchQueueRef _Nonnull self, TimerRef _Nonnull pTimer)
{
    decl_try_err();

    DispatchQueue_AddTimer_Locked(self, pTimer);
    try(DispatchQueue_AcquireVirtualProcessor_Locked(self));
    ConditionVariable_SignalAndUnlock(&self->work_available_signaler, &self->lock);

    return EOK;

catch:
    return err;
}

// Removes all scheduled instances of the given work item from the dispatch
// queue.
static void DispatchQueue_RemoveTimer_Locked(DispatchQueueRef _Nonnull self, TimerRef _Nonnull pTimer)
{
    TimerRef pCurItem = (TimerRef) self->timer_queue.first;
    TimerRef pPrevItem = NULL;

    while (pCurItem) {
        if (pCurItem == pTimer) {
            TimerRef pNextItem = (TimerRef) pCurItem->item.queue_entry.next;

            SList_Remove(&self->timer_queue, &pPrevItem->item.queue_entry, &pCurItem->item.queue_entry);
            DispatchQueue_RelinquishTimer_Locked(self, pCurItem);
            // pPrevItem doesn't change here
            pCurItem = pNextItem;
        }
        else {
            pPrevItem = pCurItem;
            pCurItem = (TimerRef) pCurItem->item.queue_entry.next;
        }
    }
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: API
////////////////////////////////////////////////////////////////////////////////

// Returns the dispatch queue that is associated with the virtual processor that
// is running the calling code. This will always return a dispatch queue for
// callers that are running in a dispatch queue context. It returns NULL though
// for callers that are running on a virtual processor that was directly acquired
// from the virtual processor pool.
DispatchQueueRef _Nullable DispatchQueue_GetCurrent(void)
{
    return (DispatchQueueRef) VirtualProcessor_GetCurrent()->dispatchQueue;
}


// Returns the process that owns the dispatch queue. Returns NULL if the dispatch
// queue is not owned by any particular process. Eg the kernel main dispatch queue.
ProcessRef _Nullable _Weak DispatchQueue_GetOwningProcess(DispatchQueueRef _Nonnull self)
{
    return self->owning_process;
}

// Sets the dispatch queue descriptor
// Not concurrency safe
void DispatchQueue_SetDescriptor(DispatchQueueRef _Nonnull self, int desc)
{
    self->descriptor = desc;
}

// Returns the dispatch queue descriptor and -1 if no descriptor has been set on
// the queue.
// Not concurrency safe
int DispatchQueue_GetDescriptor(DispatchQueueRef _Nonnull self)
{
    return self->descriptor;
}


// Synchronously executes the given closure. The closure is executed as soon as
// possible and the caller remains blocked until the closure has finished
// execution. This function returns with an EINTR if the queue is flushed or
// terminated by calling DispatchQueue_Terminate().
errno_t DispatchQueue_DispatchSync(DispatchQueueRef _Nonnull self, DispatchQueueClosure closure)
{
    decl_try_err();
    WorkItem* pItem = NULL;
    bool needsUnlock = false;

    Lock_Lock(&self->lock);
    needsUnlock = true;
    if (self->state >= kQueueState_Terminating) {
        Lock_Unlock(&self->lock);
        return EOK;
    }

    try(DispatchQueue_AcquireWorkItem_Locked(self, closure, &pItem));
    try(DispatchQueue_DispatchWorkItemSyncAndUnlock_Locked(self, pItem));
    return EOK;

catch:
    if (pItem) {
        DispatchQueue_RelinquishWorkItem_Locked(self, pItem);
    }
    if (needsUnlock) {
        Lock_Unlock(&self->lock);
    }
    return err;
}

// Asynchronously executes the given closure. The closure is executed as soon as
// possible.
errno_t DispatchQueue_DispatchAsync(DispatchQueueRef _Nonnull self, DispatchQueueClosure closure)
{
    decl_try_err();
    WorkItem* pItem = NULL;
    bool needsUnlock = false;

    Lock_Lock(&self->lock);
    needsUnlock = true;
    if (self->state >= kQueueState_Terminating) {
        Lock_Unlock(&self->lock);
        return EOK;
    }

    try(DispatchQueue_AcquireWorkItem_Locked(self, closure, &pItem));
    try(DispatchQueue_DispatchWorkItemAsyncAndUnlock_Locked(self, pItem));
    return EOK;

catch:
    if (pItem) {
        DispatchQueue_RelinquishWorkItem_Locked(self, pItem);
    }
    if (needsUnlock) {
        Lock_Unlock(&self->lock);
    }
    return err;
}

// Asynchronously executes the given closure on or after 'deadline'. The dispatch
// queue will try to execute the closure as close to 'deadline' as possible.
errno_t DispatchQueue_DispatchAsyncAfter(DispatchQueueRef _Nonnull self, TimeInterval deadline, DispatchQueueClosure closure)
{
    decl_try_err();
    Timer* pTimer = NULL;
    bool needsUnlock = false;

    Lock_Lock(&self->lock);
    needsUnlock = true;
    if (self->state >= kQueueState_Terminating) {
        Lock_Unlock(&self->lock);
        return EOK;
    }

    try(DispatchQueue_AcquireTimer_Locked(self, deadline, kTimeInterval_Zero, closure, &pTimer));
    try(DispatchQueue_DispatchTimer_Locked(self, pTimer));
    return EOK;

catch:
    if (pTimer) {
        DispatchQueue_RelinquishTimer_Locked(self, pTimer);
    }
    if (needsUnlock) {
        Lock_Unlock(&self->lock);
    }
    return err;
}


// Synchronously executes the given work item. The work item is executed as
// soon as possible and the caller remains blocked until the work item has
// finished execution. This function returns with an EINTR if the queue is
// flushed or terminated by calling DispatchQueue_Terminate().
errno_t DispatchQueue_DispatchWorkItemSync(DispatchQueueRef _Nonnull self, WorkItemRef _Nonnull pItem)
{
    decl_try_err();
    bool needsUnlock = false;

    if (AtomicBool_Set(&pItem->is_being_dispatched, true)) {
        // Some other queue is already dispatching this work item
        return EBUSY;
    }

    Lock_Lock(&self->lock);
    needsUnlock = true;
    if (self->state >= kQueueState_Terminating) {
        Lock_Unlock(&self->lock);
        return EOK;
    }

    try(DispatchQueue_DispatchWorkItemSyncAndUnlock_Locked(self, pItem));
    return EOK;

catch:
    if (needsUnlock) {
        Lock_Unlock(&self->lock);
    }
    return err;
}

// Asynchronously executes the given work item. The work item is executed as
// soon as possible.
errno_t DispatchQueue_DispatchWorkItemAsync(DispatchQueueRef _Nonnull self, WorkItemRef _Nonnull pItem)
{
    decl_try_err();
    bool needsUnlock = false;

    if (AtomicBool_Set(&pItem->is_being_dispatched, true)) {
        // Some other queue is already dispatching this work item
        return EBUSY;
    }

    Lock_Lock(&self->lock);
    needsUnlock = true;
    if (self->state >= kQueueState_Terminating) {
        Lock_Unlock(&self->lock);
        return EOK;
    }

    try(DispatchQueue_DispatchWorkItemAsyncAndUnlock_Locked(self, pItem));
    return EOK;

catch:
    if (needsUnlock) {
        Lock_Unlock(&self->lock);
    }
    return err;
}

// Removes all scheduled instances of the given work item from the dispatch queue.
// Work items are compared by their pointer identity and all items with the same
// pointer identity as 'pItem' are removed from the queue. Note that this
// function does not cancel the item nor clear the cancel state of the item if
// it is in cancelled state. If the closure of the work item is in the process
// of executing when this function is called then the closure will continue to
// execute undisturbed. If the work item however is still pending and has not
// yet executed then it will be removed and it will not execute.
// All outstanding DispatchWorkItemSync() calls on this item will return with an
// EINTR error.
void DispatchQueue_RemoveWorkItem(DispatchQueueRef _Nonnull self, WorkItemRef _Nonnull pItem)
{
    Lock_Lock(&self->lock);
    // Queue terminating state isn't relevant here
    DispatchQueue_RemoveWorkItem_Locked(self, pItem);
    Lock_Unlock(&self->lock);
}


// Asynchronously executes the given timer when it comes due.
errno_t DispatchQueue_DispatchTimer(DispatchQueueRef _Nonnull self, TimerRef _Nonnull pTimer)
{
    decl_try_err();
    bool needsUnlock = false;

    if (AtomicBool_Set(&pTimer->item.is_being_dispatched, true)) {
        // Some other queue is already dispatching this timer
        return EBUSY;
    }

    Lock_Lock(&self->lock);
    needsUnlock = true;
    if (self->state >= kQueueState_Terminating) {
        Lock_Unlock(&self->lock);
        return EOK;
    }

    try(DispatchQueue_DispatchTimer_Locked(self, pTimer));
    return EOK;

catch:
    if (needsUnlock) {
        Lock_Unlock(&self->lock);
    }
    return err;
}

// Removes all scheduled instances of the given timer from the dispatch queue.
// Timers are compared by their pointer identity and all items with the same
// pointer identity as 'pTimer' are removed from the queue. Note that this
// function does not cancel the timer nor clear the cancel state of the timer if
// it is in cancelled state. If the closure of the timer is in the process
// of executing when this function is called then the closure will continue to
// execute undisturbed. If the timer however is still pending and has not yet
// executed then it will be removed and it will not execute.
void DispatchQueue_RemoveTimer(DispatchQueueRef _Nonnull self, TimerRef _Nonnull pTimer)
{
    Lock_Lock(&self->lock);
    // Queue terminating state isn't relevant here
    DispatchQueue_RemoveTimer_Locked(self, pTimer);
    Lock_Unlock(&self->lock);
}


// Removes all queued work items, one-shot and repeatable timers from the queue.
void DispatchQueue_Flush(DispatchQueueRef _Nonnull self)
{
    Lock_Lock(&self->lock);
    DispatchQueue_Flush_Locked(self);
    Lock_Unlock(&self->lock);
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Queue Main Loop
////////////////////////////////////////////////////////////////////////////////

static void DispatchQueue_RearmTimer_Locked(DispatchQueueRef _Nonnull self, TimerRef _Nonnull pTimer)
{
    // Repeating timer: rearm it with the next fire date that's in
    // the future (the next fire date we haven't already missed).
    const TimeInterval curTime = MonotonicClock_GetCurrentTime();
                
    do  {
        pTimer->deadline = TimeInterval_Add(pTimer->deadline, pTimer->interval);
    } while (TimeInterval_Less(pTimer->deadline, curTime));
    
    DispatchQueue_AddTimer_Locked(self, pTimer);
}

void DispatchQueue_Run(DispatchQueueRef _Nonnull self)
{
    VirtualProcessor* pVP = VirtualProcessor_GetCurrent();

    // We hold the lock at all times except:
    // - while waiting for work
    // - while executing a work item
    Lock_Lock(&self->lock);

    while (true) {
        WorkItemRef pItem = NULL;
        bool mayRelinquish = false;
        
        // Wait for work items to arrive or for timers to fire
        while (true) {
            // Grab the first timer that's due. We give preference to timers because
            // they are tied to a specific deadline time while immediate work items
            // do not guarantee that they will execute at a specific time. So it's
            // acceptable to push them back on the timeline.
            Timer* pFirstTimer = (Timer*)self->timer_queue.first;
            if (pFirstTimer && TimeInterval_LessEquals(pFirstTimer->deadline, MonotonicClock_GetCurrentTime())) {
                pItem = (WorkItemRef) SList_RemoveFirst(&self->timer_queue);
            }


            // Grab the first work item if no timer is due
            if (pItem == NULL) {
                pItem = (WorkItemRef) SList_RemoveFirst(&self->item_queue);
                self->items_queued_count--;
            }



            // We're done with this loop if we got an item to execute, we're
            // supposed to terminate or we got no item and it's okay to
            // relinquish this VP
            if (pItem != NULL || self->state >= kQueueState_Terminating || (pItem == NULL && mayRelinquish)) {
                break;
            }
            

            // Compute a deadline for the wait. We do not wait if the deadline
            // is equal to the current time or it's in the past
            TimeInterval deadline;

            if (self->timer_queue.first) {
                deadline = ((TimerRef)self->timer_queue.first)->deadline;
            } else {
                deadline = TimeInterval_Add(MonotonicClock_GetCurrentTime(), TimeInterval_MakeSeconds(2));
            }


            // Wait for work. This drops the queue lock while we're waiting. This
            // call may return with a ETIMEDOUT error. This is fine. Either some
            // new work has arrived in the meantime or if not then we are free
            // to relinquish the VP since it hasn't done anything useful for a
            // longer time.
            const int err = ConditionVariable_Wait(&self->work_available_signaler, &self->lock, deadline);
            if (err == ETIMEDOUT && self->availableConcurrency > self->minConcurrency) {
                mayRelinquish = true;
            }
        }

        
        // Relinquish this VP if we did not get an item to execute or the queue
        // is terminating
        if (pItem == NULL || self->state >= kQueueState_Terminating) {
            break;
        }


        // Drop the lock. We do not want to hold it while the closure is executing
        // and we are (if needed) signaling completion.
        Lock_Unlock(&self->lock);


        // Execute the work item
        if (pItem->closure.isUser) {
            VirtualProcessor_CallAsUser(pVP, pItem->closure.func, pItem->closure.context);
        } else {
            pItem->closure.func(pItem->closure.context);
        }

        // Signal the work item's completion semaphore if needed
        if (pItem->completion != NULL) {
            WorkItem_SignalCompletion(pItem, false);
        }


        // Reacquire the lock
        Lock_Lock(&self->lock);


        // Move the work item back to the item cache if possible or destroy it
        switch (pItem->type) {
            case kItemType_Immediate:
                DispatchQueue_RelinquishWorkItem_Locked(self, pItem);
                break;
                
            case kItemType_OneShotTimer:
                DispatchQueue_RelinquishTimer_Locked(self, (TimerRef) pItem);
                break;
                
            case kItemType_RepeatingTimer: {
                Timer* pTimer = (TimerRef)pItem;
                
                if (pTimer->item.cancelled) {
                    DispatchQueue_RelinquishTimer_Locked(self, pTimer);
                } else if (self->state == kQueueState_Running) {
                    DispatchQueue_RearmTimer_Locked(self, pTimer);
                }
                break;
            }
                
            default:
                abort();
                break;
        }
    }

    DispatchQueue_RelinquishVirtualProcessor_Locked(self, pVP);

    if (self->state >= kQueueState_Terminating) {
        ConditionVariable_SignalAndUnlock(&self->vp_shutdown_signaler, &self->lock);
    } else {
        Lock_Unlock(&self->lock);
    }
}
