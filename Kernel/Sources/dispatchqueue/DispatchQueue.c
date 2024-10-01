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
    self->item_cache_capacity = __max(MAX_ITEM_CACHE_COUNT, maxConcurrency);
    self->completion_signaler_capacity = MAX_COMPLETION_SIGNALER_CACHE_COUNT;

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
    WorkItem* pItem;

    // Flush the work item queue
    while ((pItem = (WorkItem*) SList_RemoveFirst(&self->item_queue)) != NULL) {
        DispatchQueue_SignalWorkItemCompletion(self, pItem, true);
        DispatchQueue_RelinquishWorkItem_Locked(self, pItem);
    }


    // Flush the timed items
    while ((pItem = (WorkItem*) SList_RemoveFirst(&self->timer_queue)) != NULL) {
        DispatchQueue_RelinquishWorkItem_Locked(self, pItem);
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
    CompletionSignaler* pComp;
    WorkItem* pItem;

    assert(self->state == kQueueState_Terminated);

    // No more VPs are attached to this queue. We can now go ahead and free
    // all resources.
    SList_Deinit(&self->item_queue);      // guaranteed to be empty at this point
    SList_Deinit(&self->timer_queue);     // guaranteed to be empty at this point

    self->item_cache_capacity = 0;  // Force relinquish to deallocate
    while ((pItem = (WorkItem*) SList_RemoveFirst(&self->item_cache_queue)) != NULL) {
        DispatchQueue_RelinquishWorkItem_Locked(self, pItem);
    }
    SList_Deinit(&self->item_cache_queue);
    
    self->completion_signaler_capacity = 0;
    while((pComp = (CompletionSignaler*) SList_RemoveFirst(&self->completion_signaler_cache_queue)) != NULL) {
        DispatchQueue_RelinquishCompletionSignaler_Locked(self, pComp);
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
        self->concurrency_lanes[conLaneIdx].active_item = NULL;
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
    const int conLaneIdx = pVP->dispatchQueueConcurrencyLaneIndex;

    assert(conLaneIdx >= 0 && conLaneIdx < self->maxConcurrency);

    VirtualProcessor_SetDispatchQueue(pVP, NULL, -1);
    self->concurrency_lanes[conLaneIdx].vp = NULL;
    self->concurrency_lanes[conLaneIdx].active_item = NULL;
    self->availableConcurrency--;
}


// Creates a work item for the given closure and closure context. Tries to reuse
// an existing work item from the work item cache whenever possible. Expects that
// the caller holds the dispatch queue lock.
static errno_t DispatchQueue_AcquireWorkItem_Locked(DispatchQueueRef _Nonnull self, Closure1Arg_Func _Nonnull func, void* _Nullable context, uintptr_t tag, WorkItem* _Nullable * _Nonnull pOutItem)
{
    WorkItem* pItem = NULL;

    if (self->item_cache_queue.first != NULL) {
        pItem = (WorkItem*) SList_RemoveFirst(&self->item_cache_queue);
        self->item_cache_count--;
    }
    else {
        const errno_t err = kalloc(sizeof(WorkItem), (void**) &pItem);

        if (err != EOK) {
            *pOutItem = NULL;
            return err;
        }
    }


    // (Re-)Initialize the work item
    SListNode_Init(&pItem->queue_entry);
    pItem->func = func;
    pItem->context = context;
    pItem->completion = NULL;
    pItem->tag = tag;
    pItem->flags = 0;

    *pOutItem = pItem;
    return EOK;
}

// Relinquishes the given work item. A work item owned by the dispatch queue is
// moved back to the item reuse cache if possible or freed if the cache is full.
// Does nothing if the dispatch queue does not own the item.
static void DispatchQueue_RelinquishWorkItem_Locked(DispatchQueueRef _Nonnull self, WorkItem* _Nonnull pItem)
{
    SListNode_Deinit(&pItem->queue_entry);
    pItem->func = NULL;
    pItem->context = NULL;
    pItem->completion = NULL;
    pItem->tag = 0;
    pItem->flags = 0;

    if (self->item_cache_count < self->item_cache_capacity) {
        SList_InsertBeforeFirst(&self->item_cache_queue, &pItem->queue_entry);
        self->item_cache_count++;
    }
    else {
        kfree(pItem);
    }
}

// Creates a completion signaler. Tries to reuse an existing completion signaler
// from the completion signaler cache whenever possible. Expects that
// the caller holds the dispatch queue lock.
static errno_t DispatchQueue_AcquireCompletionSignaler_Locked(DispatchQueueRef _Nonnull self, CompletionSignaler* _Nullable * _Nonnull pOutComp)
{
    CompletionSignaler* pComp = NULL;

    if (self->completion_signaler_cache_queue.first != NULL) {
        pComp = (CompletionSignaler*) SList_RemoveFirst(&self->completion_signaler_cache_queue);
        self->completion_signaler_count--;
    }
    else {
        const errno_t err = kalloc(sizeof(CompletionSignaler), (void**) &pComp);

        if (err != EOK) {
            *pOutComp = NULL;
            return err;
        }
        Semaphore_Init(&pComp->semaphore, 0);
    }


    // (Re-)Initialize the completion signaler
    SListNode_Init(&pComp->queue_entry);
    pComp->isInterrupted = false;

    *pOutComp = pComp;
    return EOK;
}

// Relinquishes the given completion signaler back to the completion signaler
// cache if possible. The completion signaler is freed if the cache is at capacity.
static void DispatchQueue_RelinquishCompletionSignaler_Locked(DispatchQueueRef _Nonnull self, CompletionSignaler* _Nonnull pComp)
{
    SListNode_Deinit(&pComp->queue_entry);
    pComp->isInterrupted = false;

    if (self->completion_signaler_count < self->completion_signaler_capacity) {
        SList_InsertBeforeFirst(&self->completion_signaler_cache_queue, &pComp->queue_entry);
        self->completion_signaler_count++;
    }
    else {
        Semaphore_Deinit(&pComp->semaphore);
        kfree(pComp);
    }
}

// Signals the completion of a work item. State is protected by the dispatch
// queue lock. The 'isInterrupted' parameter indicates whether the item should
// be considered interrupted or finished.
static void DispatchQueue_SignalWorkItemCompletion(DispatchQueueRef _Nonnull self, WorkItem* _Nonnull pItem, bool isInterrupted)
{
    if (pItem->completion != NULL) {
        pItem->completion->isInterrupted = isInterrupted;
        Semaphore_Relinquish(&pItem->completion->semaphore);
        pItem->completion = NULL;
    }
}

// Adds the given work item to the immediate work item queue. Returns the item
// that comes before the newly inserted one.
static WorkItem* _Nullable DispatchQueue_AddWorkItem_Locked(DispatchQueueRef _Nonnull self, WorkItem* _Nonnull pItem)
{
    WorkItem* pOldLastItem = (WorkItem*)self->item_queue.last;

    SList_InsertAfterLast(&self->item_queue, &pItem->queue_entry);
    self->items_queued_count++;

    return pOldLastItem;
}

// Removes all scheduled instances of the given work item from the given item
// queue.
static bool DispatchQueue_RemoveWorkItem_Locked(DispatchQueueRef _Nonnull self, SList* _Nonnull itemQueue, uintptr_t tag)
{
    WorkItem* pCurItem = (WorkItem*) itemQueue->first;
    WorkItem* pPrevItem = NULL;
    bool didRemove = false;

    while (pCurItem) {
        WorkItem* pNextItem = (WorkItem*) pCurItem->queue_entry.next;

        if (pCurItem->tag == tag) {
            if (pCurItem->completion) {
                DispatchQueue_SignalWorkItemCompletion(self, pCurItem, true);
            }

            SList_Remove(itemQueue, &pPrevItem->queue_entry, &pCurItem->queue_entry);
            self->items_queued_count--;

            DispatchQueue_RelinquishWorkItem_Locked(self, pCurItem);
            didRemove = true;
            // pPrevItem doesn't change here
        }
        else {
            pPrevItem = pCurItem;
        }
        pCurItem = pNextItem;
    }

    return didRemove;
}

// Adds the given work item to the timer queue. Expects that the queue is already
// locked. Does not wake up the queue. Returns the item that comes before the
// newly inserted one.
static WorkItem* _Nullable DispatchQueue_AddTimedItem_Locked(DispatchQueueRef _Nonnull self, WorkItem* _Nonnull pItem)
{
    WorkItem* pPrevItem = NULL;
    WorkItem* pCurItem = (WorkItem*)self->timer_queue.first;
    
    while (pCurItem) {
        if (TimeInterval_Greater(pCurItem->u.timer.deadline, pItem->u.timer.deadline)) {
            break;
        }
        
        pPrevItem = pCurItem;
        pCurItem = (WorkItem*)pCurItem->queue_entry.next;
    }
    
    SList_InsertAfter(&self->timer_queue, &pItem->queue_entry, &pPrevItem->queue_entry);
    self->items_queued_count++;

    return pPrevItem;
}

// Returns true if an item (or timer) with the given tag is queued or currently
// executing.
static bool DispatchQueue_HasItemWithTag_Locked(DispatchQueueRef _Nonnull self, uintptr_t tag)
{
    // Check the currently executing items
    for (int i = 0; i < self->maxConcurrency; i++) {
        WorkItem* pItem = self->concurrency_lanes[i].active_item;

        if (pItem && pItem->tag == tag) {
            return true;
        }
    }


    // Check immediate items
    SList_ForEach(&self->item_queue, WorkItem,
        if (pCurNode->tag == tag) {
            return true;
        }
    );


    // Check timers
    SList_ForEach(&self->timer_queue, WorkItem,
        if (pCurNode->tag == tag) {
            return true;
        }
    );

    return false;
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
errno_t DispatchQueue_DispatchSync(DispatchQueueRef _Nonnull self, Closure1Arg_Func _Nonnull func, void* _Nullable context)
{
    return DispatchQueue_DispatchClosure(self, func, context, kDispatchOption_Sync, 0);
}

// Asynchronously executes the given closure. The closure is executed as soon as
// possible.
errno_t DispatchQueue_DispatchAsync(DispatchQueueRef _Nonnull self, Closure1Arg_Func _Nonnull func, void* _Nullable context)
{
    return DispatchQueue_DispatchClosure(self, func, context, 0, 0);
}

// Dispatches 'func' on the dispatch queue. 'func' will be called with 'context'
// as the first argument. Use 'options to control whether the function should
// be executed in kernel or user space and whether the caller should be blocked
// until 'func' has finished executing. The function will execute as soon as
// possible.
errno_t DispatchQueue_DispatchClosure(DispatchQueueRef _Nonnull self, Closure1Arg_Func _Nonnull func, void* _Nullable context, uint32_t options, uintptr_t tag)
{
    decl_try_err();
    WorkItem* pItem = NULL;
    CompletionSignaler* pComp = NULL;
    const bool isSync = (options & kDispatchOption_Sync) == kDispatchOption_Sync;
    bool isLocked = false;

    Lock_Lock(&self->lock);
    isLocked = true;
    if (self->state >= kQueueState_Terminating) {
        Lock_Unlock(&self->lock);
        return EOK;
    }


    if ((options & kDispatchOption_Coalesce) == kDispatchOption_Coalesce) {
        if (DispatchQueue_HasItemWithTag_Locked(self, tag)) {
            Lock_Unlock(&self->lock);
            return EOK;
        }
    }


    try(DispatchQueue_AcquireWorkItem_Locked(self, func, context, tag, &pItem));
    if ((options & kDispatchOption_User) == kDispatchOption_User) {
        pItem->flags |= kWorkItemFlag_IsUser;
    }


    if (isSync) {
        try(DispatchQueue_AcquireCompletionSignaler_Locked(self, &pComp));

        // The work item maintains a weak reference to the cached completion semaphore
        pItem->completion = pComp;
    }


    // Queue the work item and wake a VP
    WorkItem* pOldLastItem = DispatchQueue_AddWorkItem_Locked(self, pItem);
    err = DispatchQueue_AcquireVirtualProcessor_Locked(self);
    if (err != EOK) {
        SList_Remove(&self->item_queue, &pOldLastItem->queue_entry, &pItem->queue_entry);
        throw(err);
    }

    ConditionVariable_SignalAndUnlock(&self->work_available_signaler, &self->lock);
    isLocked = false;
    // Queue is now unlocked


    if (isSync) {
        // Wait for the work item completion
        err = Semaphore_Acquire(&pComp->semaphore, kTimeInterval_Infinity);

        Lock_Lock(&self->lock);

        if ((err == EOK && pComp->isInterrupted) || self->state >= kQueueState_Terminating) {
            // We want to return EINTR if the sync dispatch was interrupted by a
            // DispatchQueue_Terminate()
            err = EINTR;
        }

        DispatchQueue_RelinquishCompletionSignaler_Locked(self, pComp);
        Lock_Unlock(&self->lock);
    }

    return err;

catch:
    if (!isLocked) {
        Lock_Lock(&self->lock);
        isLocked = true;
    }
    if (pComp) {
        DispatchQueue_RelinquishCompletionSignaler_Locked(self, pComp);
    }
    if (pItem) {
        DispatchQueue_RelinquishWorkItem_Locked(self, pItem);
    }
    if (isLocked) {
        Lock_Unlock(&self->lock);
    }

    return err;
}


// Asynchronously executes the given closure on or after 'deadline'. The dispatch
// queue will try to execute the closure as close to 'deadline' as possible. The
// timer can be referenced with the tag 'tag'.
errno_t DispatchQueue_DispatchAsyncAfter(DispatchQueueRef _Nonnull self, TimeInterval deadline, Closure1Arg_Func _Nonnull func, void* _Nullable context, uintptr_t tag)
{
    return DispatchQueue_DispatchTimer(self, deadline, kTimeInterval_Zero, func, context, 0, tag);
}

// Asynchronously executes the given closure every 'interval' seconds, on or
// after 'deadline' until the timer is removed from the dispatch queue. The
// timer can be referenced with the tag 'tag'.
errno_t DispatchQueue_DispatchAsyncPeriodically(DispatchQueueRef _Nonnull self, TimeInterval deadline, TimeInterval interval, Closure1Arg_Func _Nonnull func, void* _Nullable context, uintptr_t tag)
{
    return DispatchQueue_DispatchTimer(self, deadline, interval, func, context, 0, tag);
}

// Similar to 'DispatchClosure'. However the function will execute on or after
// 'deadline'. If 'interval' is not 0 or infinity, then the function will execute
// every 'interval' ticks until the timer is removed from the queue.
errno_t DispatchQueue_DispatchTimer(DispatchQueueRef _Nonnull self, TimeInterval deadline, TimeInterval interval, Closure1Arg_Func _Nonnull func, void* _Nullable context, uint32_t options, uintptr_t tag)
{
    decl_try_err();
    WorkItem* pItem = NULL;

    if ((options & kDispatchOption_Sync) == kDispatchOption_Sync) {
        return EINVAL;
    }

    Lock_Lock(&self->lock);
    if (self->state >= kQueueState_Terminating) {
        Lock_Unlock(&self->lock);
        return EOK;
    }


    if ((options & kDispatchOption_Coalesce) == kDispatchOption_Coalesce) {
        if (DispatchQueue_HasItemWithTag_Locked(self, tag)) {
            Lock_Unlock(&self->lock);
            return EOK;
        }
    }


    try(DispatchQueue_AcquireWorkItem_Locked(self, func, context, tag, &pItem));

    pItem->u.timer.deadline = deadline;
    pItem->u.timer.interval = interval;
    pItem->flags |= kWorkItemFlag_Timer;

    if ((options & kDispatchOption_User) == kDispatchOption_User) {
        pItem->flags |= kWorkItemFlag_IsUser;
    }
    if (TimeInterval_Greater(interval, kTimeInterval_Zero) && !TimeInterval_Equals(interval, kTimeInterval_Infinity)) {
        pItem->flags |= kWorkItemFlag_IsRepeating;
    }


    WorkItem* pPrevItem = DispatchQueue_AddTimedItem_Locked(self, pItem);
    err = DispatchQueue_AcquireVirtualProcessor_Locked(self);
    if (err != EOK) {
        SList_Remove(&self->timer_queue, &pPrevItem->queue_entry, &pItem->queue_entry);
        throw(err);
    }

    ConditionVariable_SignalAndUnlock(&self->work_available_signaler, &self->lock);

    return EOK;

catch:
    if (pItem) {
        DispatchQueue_RelinquishWorkItem_Locked(self, pItem);
    }
    Lock_Unlock(&self->lock);

    return err;
}


// Removes all scheduled instances of timers and immediate work items with tag
// 'tag' from the dispatch queue. If the closure of the work item is in the
// process of executing when this function is called then the closure will
// continue to execute uninterrupted. If on the other side, the work item is
// still pending and has not executed yet then it will be removed and it will
// not execute.
bool DispatchQueue_RemoveByTag(DispatchQueueRef _Nonnull self, uintptr_t tag)
{
    Lock_Lock(&self->lock);
    // Queue termination state isn't relevant here
    const bool r0 = DispatchQueue_RemoveWorkItem_Locked(self, &self->item_queue, tag);
    const bool r1 = DispatchQueue_RemoveWorkItem_Locked(self, &self->timer_queue, tag);
    Lock_Unlock(&self->lock);
    return (r0 || r1) ? true : false;
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

static void DispatchQueue_RearmTimedItem_Locked(DispatchQueueRef _Nonnull self, WorkItem* _Nonnull pItem)
{
    // Repeating timer: rearm it with the next fire date that's in
    // the future (the next fire date we haven't already missed).
    const TimeInterval curTime = MonotonicClock_GetCurrentTime();
    
    do  {
        pItem->u.timer.deadline = TimeInterval_Add(pItem->u.timer.deadline, pItem->u.timer.interval);
    } while (TimeInterval_Less(pItem->u.timer.deadline, curTime));
    
    DispatchQueue_AddTimedItem_Locked(self, pItem);
}

void DispatchQueue_Run(DispatchQueueRef _Nonnull self)
{
    VirtualProcessor* pVP = VirtualProcessor_GetCurrent();
    ConcurrencyLane* pConLane = &self->concurrency_lanes[pVP->dispatchQueueConcurrencyLaneIndex];


    // We hold the lock at all times except:
    // - while waiting for work
    // - while executing a work item
    Lock_Lock(&self->lock);

    while (true) {
        WorkItem* pItem = NULL;
        bool mayRelinquish = false;
        
        // Wait for work items to arrive or for timers to fire
        while (true) {
            const TimeInterval now = MonotonicClock_GetCurrentTime();

            // Grab the first timer that's due. We give preference to timers because
            // they are tied to a specific deadline time while immediate work items
            // do not guarantee that they will execute at a specific time. So it's
            // acceptable to push them back on the timeline.
            WorkItem* pFirstTimer = (WorkItem*)self->timer_queue.first;
            if (pFirstTimer && TimeInterval_LessEquals(pFirstTimer->u.timer.deadline, now)) {
                pItem = (WorkItem*) SList_RemoveFirst(&self->timer_queue);
                self->items_queued_count--;
            }


            // Grab the first work item if no timer is due
            if (pItem == NULL) {
                pItem = (WorkItem*) SList_RemoveFirst(&self->item_queue);
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
                deadline = ((WorkItem*)self->timer_queue.first)->u.timer.deadline;
            } else {
                deadline = TimeInterval_Add(now, TimeInterval_MakeSeconds(2));
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


        // Make the item the active item
        pConLane->active_item = pItem;


        // Drop the lock. We do not want to hold it while the closure is executing
        // and we are (if needed) signaling completion.
        Lock_Unlock(&self->lock);


        // Execute the work item
        if ((pItem->flags & kWorkItemFlag_IsUser) == kWorkItemFlag_IsUser) {
            VirtualProcessor_CallAsUser(pVP, pItem->func, pItem->context);
        } else {
            pItem->func(pItem->context);
        }

        // Signal the work item's completion semaphore if needed
        if (pItem->completion != NULL) {
            DispatchQueue_SignalWorkItemCompletion(self, pItem, false);
        }


        // Reacquire the lock
        Lock_Lock(&self->lock);


        // Deactivate the item
        pConLane->active_item = NULL;


        // Move the work item back to the item cache if possible or destroy it
        if ((pItem->flags & kWorkItemFlag_IsRepeating) == kWorkItemFlag_IsRepeating && self->state == kQueueState_Running) {
            DispatchQueue_RearmTimedItem_Locked(self, pItem);
        }
        else {
            DispatchQueue_RelinquishWorkItem_Locked(self, pItem);
        }
    }

    DispatchQueue_RelinquishVirtualProcessor_Locked(self, pVP);

    if (self->state >= kQueueState_Terminating) {
        ConditionVariable_SignalAndUnlock(&self->vp_shutdown_signaler, &self->lock);
    } else {
        Lock_Unlock(&self->lock);
    }
}
