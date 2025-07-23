//
//  DispatchQueue.c
//  kernel
//
//  Created by Dietmar Planitzer on 4/21/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "DispatchQueuePriv.h"
#include <machine/clock.h>
#include <kern/kalloc.h>
#include <kern/string.h>
#include <kern/timespec.h>


errno_t DispatchQueue_Create(int minConcurrency, int maxConcurrency, int qos, int priority, vcpu_pool_t _Nonnull vpPoolRef, ProcessRef _Nullable _Weak pProc, DispatchQueueRef _Nullable * _Nonnull pOutQueue)
{
    decl_try_err();
    DispatchQueueRef self = NULL;

    if (maxConcurrency < 1 || maxConcurrency > INT8_MAX) {
        throw(EINVAL);
    }
    if (minConcurrency < 0 || minConcurrency > maxConcurrency) {
        throw(EINVAL);
    }
    
    try(Object_Create(class(DispatchQueue), sizeof(ConcurrencyLane) * (maxConcurrency - 1), (void**)&self));
    SList_Init(&self->item_queue);
    SList_Init(&self->timer_queue);
    SList_Init(&self->item_cache_queue);
    mtx_init(&self->lock);
    cnd_init(&self->work_available_signaler);
    cnd_init(&self->vp_shutdown_signaler);
    self->owning_process = pProc;
    self->descriptor = -1;
    self->virtual_processor_pool = vpPoolRef;
    self->state = kQueueState_Running;
    self->minConcurrency = (int8_t)minConcurrency;
    self->maxConcurrency = (int8_t)maxConcurrency;
    self->qos = qos;
    self->priority = priority;
    self->item_cache_capacity = __max(MAX_ITEM_CACHE_COUNT, maxConcurrency);

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
    mtx_lock(&self->lock);
    if (self->state >= kQueueState_Terminating) {
        mtx_unlock(&self->lock);
        return;
    }
    self->state = kQueueState_Terminating;


    // Flush the dispatch queue which means that we get rid of all still queued
    // work items and timers.
    DispatchQueue_Flush_Locked(self);


    // We want to wake _all_ VPs up here since all of them need to relinquish
    // themselves.
    cnd_broadcast(&self->work_available_signaler);
    mtx_unlock(&self->lock);
}

// Waits until the dispatch queue has reached 'terminated' state which means that
// all VPs have been relinquished.
void DispatchQueue_WaitForTerminationCompleted(DispatchQueueRef _Nonnull self)
{
    mtx_lock(&self->lock);
    while (self->availableConcurrency > 0) {
        cnd_wait(&self->vp_shutdown_signaler, &self->lock);
    }


    // The queue is now in terminated state
    self->state = kQueueState_Terminated;
    mtx_unlock(&self->lock);
}

// Deallocates the dispatch queue. Expects that the queue is in 'terminated' state.
static void _DispatchQueue_Destroy(DispatchQueueRef _Nonnull self)
{
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
    
    mtx_deinit(&self->lock);
    cnd_deinit(&self->work_available_signaler);
    cnd_deinit(&self->vp_shutdown_signaler);
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
static void DispatchQueue_AttachVirtualProcessor_Locked(DispatchQueueRef _Nonnull self, vcpu_t _Nonnull vp, int conLaneIdx)
{
    assert(conLaneIdx >= 0 && conLaneIdx < self->maxConcurrency);

    vcpu_setdq(vp, self, conLaneIdx);
    self->concurrency_lanes[conLaneIdx].vp = vp;
    self->concurrency_lanes[conLaneIdx].active_item = NULL;
    self->availableConcurrency++;
}

// Makes sure that we have enough virtual processors attached to the dispatch queue
// and acquires a virtual processor from the virtual processor pool if necessary.
// The virtual processor is attached to the dispatch queue and remains attached
// until it is relinquished by the queue.
static errno_t DispatchQueue_AcquireVirtualProcessor_Locked(DispatchQueueRef _Nonnull self)
{
    decl_try_err();
    vcpu_t vp = NULL;
    VirtualProcessorParameters params;
    static AtomicInt gNextAvailVcpuid = 2;

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

        params.func = (VoidFunc_1)DispatchQueue_Run;
        params.context = self;
        params.ret_func = NULL;
        params.kernelStackSize = VP_DEFAULT_KERNEL_STACK_SIZE;
        params.userStackSize = VP_DEFAULT_USER_STACK_SIZE;
        params.id = AtomicInt_Increment(&gNextAvailVcpuid);
        params.groupid = VCPUID_MAIN_GROUP;
        params.priority = self->qos * kDispatchPriority_Count + (self->priority + kDispatchPriority_Count / 2) + VP_PRIORITIES_RESERVED_LOW;
        params.isUser = false;

        err = vcpu_pool_acquire(
                                            self->virtual_processor_pool,
                                            &params,
                                            &vp);
        if (err == EOK) {
            DispatchQueue_AttachVirtualProcessor_Locked(self, vp, conLaneIdx);
            vcpu_resume(vp, false);
        }
    }

    return err;
}

// Detaches the given virtual processor. The associated concurrency lane is
// freed up and the virtual processor is returned to the virtual processor pool
// after it has been detached from the dispatch queue. This method should only
// be called right before returning from the Dispatch_Run() method which is the
// method that runs on the virtual processor to execute work items.
static void DispatchQueue_DetachVirtualProcessor_Locked(DispatchQueueRef _Nonnull self, vcpu_t _Nonnull vp)
{
    const int conLaneIdx = vp->dispatchQueueConcurrencyLaneIndex;

    assert(conLaneIdx >= 0 && conLaneIdx < self->maxConcurrency);

    vcpu_setdq(vp, NULL, -1);
    self->concurrency_lanes[conLaneIdx].vp = NULL;
    self->concurrency_lanes[conLaneIdx].active_item = NULL;
    self->availableConcurrency--;
}


// Creates a work item for the given closure and closure context. Tries to reuse
// an existing work item from the work item cache whenever possible. Expects that
// the caller holds the dispatch queue lock.
static errno_t DispatchQueue_AcquireWorkItem_Locked(DispatchQueueRef _Nonnull self, VoidFunc_2 _Nonnull func, void* _Nullable context, void* _Nullable args, size_t nArgBytes, uintptr_t tag, WorkItem* _Nullable * _Nonnull pOutItem)
{
    WorkItem* pItem = NULL;
    WorkItem* pPrevItem = NULL;
    WorkItem* pCurItem = (WorkItem*) self->item_cache_queue.first;

    while (pCurItem) {
        if (pCurItem->args_byte_size >= nArgBytes) {
            SList_Remove(&self->item_cache_queue, &pPrevItem->queue_entry, &pCurItem->queue_entry);
            self->item_cache_count--;
            pItem = pCurItem;
            break;
        }
        pPrevItem = pCurItem;
        pCurItem = (WorkItem*) pCurItem->queue_entry.next;
    }

    if (pItem == NULL) {
        const size_t itemSize = (nArgBytes == 0) ? sizeof(WorkItem) : __Ceil_PowerOf2(sizeof(WorkItem), ARG_WORD_SIZE) + __Ceil_PowerOf2(nArgBytes, ARG_WORD_SIZE);
        const errno_t err = kalloc(itemSize, (void**) &pItem);

        if (err != EOK) {
            *pOutItem = NULL;
            return err;
        }
    }


    // (Re-)Initialize the work item
    SListNode_Init(&pItem->queue_entry);
    pItem->func = func;
    pItem->context = context;
    pItem->arg = (nArgBytes == 0) ? args : (void*)((uintptr_t)pItem + __Ceil_PowerOf2(sizeof(WorkItem), ARG_WORD_SIZE));
    pItem->tag = tag;
    pItem->args_byte_size = __Ceil_PowerOf2(nArgBytes, ARG_WORD_SIZE);
    pItem->flags = 0;

    if (nArgBytes > 0) {
        memcpy(pItem->arg, args, nArgBytes);
    }

    *pOutItem = pItem;
    return EOK;
}

// Relinquishes the given work item. A work item owned by the dispatch queue is
// moved back to the item reuse cache if possible or freed if the cache is full.
// Does nothing if the dispatch queue does not own the item.
static void DispatchQueue_RelinquishWorkItem_Locked(DispatchQueueRef _Nonnull self, WorkItem* _Nonnull pItem)
{
    if ((pItem->flags & kWorkItemFlag_IsSync) == kWorkItemFlag_IsSync) {
        sem_deinit(&pItem->u.completionSignaler);
    }

    SListNode_Deinit(&pItem->queue_entry);
    pItem->func = NULL;
    pItem->context = NULL;
    pItem->arg = NULL;
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

// Signals the completion of a work item. State is protected by the dispatch
// queue lock. The 'isInterrupted' parameter indicates whether the item should
// be considered interrupted or finished.
static void DispatchQueue_SignalWorkItemCompletion(DispatchQueueRef _Nonnull self, WorkItem* _Nonnull pItem, bool isInterrupted)
{
    if ((pItem->flags & kWorkItemFlag_IsSync) == kWorkItemFlag_IsSync) {
        if (isInterrupted) {
            pItem->flags |= kWorkItemFlag_IsInterrupted;
        }
        else {
            pItem->flags &= ~kWorkItemFlag_IsInterrupted;
        }
        sem_relinquish(&pItem->u.completionSignaler);
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
            DispatchQueue_SignalWorkItemCompletion(self, pCurItem, true);

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
        if (timespec_gt(&pCurItem->u.timer.deadline, &pItem->u.timer.deadline)) {
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
    return (DispatchQueueRef) vcpu_current()->dispatchQueue;
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
errno_t DispatchQueue_DispatchSync(DispatchQueueRef _Nonnull self, VoidFunc_1 _Nonnull func, void* _Nullable context)
{
    return DispatchQueue_DispatchClosure(self, (VoidFunc_2)func, context, NULL, 0, kDispatchOption_Sync, 0);
}

// Same as above but takes additional arguments of 'nArgBytes' size (in bytes).
errno_t DispatchQueue_DispatchSyncArgs(DispatchQueueRef _Nonnull self, VoidFunc_2 _Nonnull func, void* _Nullable context, void* _Nullable args, size_t nArgBytes)
{
    return DispatchQueue_DispatchClosure(self, func, context, args, nArgBytes, kDispatchOption_Sync, 0);
}

// Asynchronously executes the given closure. The closure is executed as soon as
// possible.
errno_t DispatchQueue_DispatchAsync(DispatchQueueRef _Nonnull self, VoidFunc_1 _Nonnull func, void* _Nullable context)
{
    return DispatchQueue_DispatchClosure(self, (VoidFunc_2)func, context, NULL, 0, 0, 0);
}

// Same as above but takes additional arguments of 'nArgBytes' size (in bytes).
errno_t DispatchQueue_DispatchAsyncArgs(DispatchQueueRef _Nonnull self, VoidFunc_2 _Nonnull func, void* _Nullable context, void* _Nullable args, size_t nArgBytes)
{
    return DispatchQueue_DispatchClosure(self, func, context, args, nArgBytes, 0, 0);
}

// Dispatches 'func' on the dispatch queue. 'func' will be called with 'context'
// as the first argument and a pointer to the arguments as the second argument.
// The argument pointer will be exactly 'args' if 'nArgBytes' is 0 and otherwise
// it will point to a copy of the arguments that 'args' pointed to.
// Use 'options to control whether the function should be blocked until 'func'
// has finished executing. The function will execute as soon as possible.
errno_t DispatchQueue_DispatchClosure(DispatchQueueRef _Nonnull self, VoidFunc_2 _Nonnull func, void* _Nullable context, void* _Nullable args, size_t nArgBytes, uint32_t options, uintptr_t tag)
{
    decl_try_err();
    WorkItem* pItem = NULL;
    const bool isSync = (options & kDispatchOption_Sync) == kDispatchOption_Sync;
    bool isLocked = false;

    if (nArgBytes > MAX_ARG_BYTES) {
        return EINVAL;
    }


    mtx_lock(&self->lock);
    isLocked = true;
    if (self->state >= kQueueState_Terminating) {
        mtx_unlock(&self->lock);
        return ETERMINATED;
    }


    if ((options & kDispatchOption_Coalesce) == kDispatchOption_Coalesce) {
        if (DispatchQueue_HasItemWithTag_Locked(self, tag)) {
            mtx_unlock(&self->lock);
            return EOK;
        }
    }


    try(DispatchQueue_AcquireWorkItem_Locked(self, func, context, args, nArgBytes, tag, &pItem));


    if (isSync) {
        // We tell the executing VP to NOT relinquish the item because we will
        // do it here once the item has signaled completion
        sem_init(&pItem->u.completionSignaler, 0);
        pItem->flags |= kWorkItemFlag_IsSync;
    }


    // Queue the work item and wake a VP
    WorkItem* pOldLastItem = DispatchQueue_AddWorkItem_Locked(self, pItem);
    err = DispatchQueue_AcquireVirtualProcessor_Locked(self);
    if (err != EOK) {
        SList_Remove(&self->item_queue, &pOldLastItem->queue_entry, &pItem->queue_entry);
        throw(err);
    }

    cnd_signal(&self->work_available_signaler);
    mtx_unlock(&self->lock);
    isLocked = false;
    // Queue is now unlocked


    if (isSync) {
        // Wait for the work item completion
        err = sem_acquire(&pItem->u.completionSignaler, &TIMESPEC_INF);

        mtx_lock(&self->lock);

        if (err == EOK) {
            if ((pItem->flags & kWorkItemFlag_IsInterrupted) == kWorkItemFlag_IsInterrupted) {
                err = EINTR;
            }
            else if (self->state >= kQueueState_Terminating) {
                err = ETERMINATED;
            }
        }

        DispatchQueue_RelinquishWorkItem_Locked(self, pItem);
        mtx_unlock(&self->lock);
    }

    return err;

catch:
    if (!isLocked) {
        mtx_lock(&self->lock);
        isLocked = true;
    }
    if (pItem) {
        DispatchQueue_RelinquishWorkItem_Locked(self, pItem);
    }
    if (isLocked) {
        mtx_unlock(&self->lock);
    }

    return err;
}


// Asynchronously executes the given closure on or after 'deadline'. The dispatch
// queue will try to execute the closure as close to 'deadline' as possible. The
// timer can be referenced with the tag 'tag'.
errno_t DispatchQueue_DispatchAsyncAfter(DispatchQueueRef _Nonnull self, const struct timespec* _Nonnull deadline, VoidFunc_1 _Nonnull func, void* _Nullable context, uintptr_t tag)
{
    return DispatchQueue_DispatchTimer(self, deadline, &TIMESPEC_ZERO, (VoidFunc_2)func, context, NULL, 0, 0, tag);
}

// Asynchronously executes the given closure every 'interval' seconds, on or
// after 'deadline' until the timer is removed from the dispatch queue. The
// timer can be referenced with the tag 'tag'.
errno_t DispatchQueue_DispatchAsyncPeriodically(DispatchQueueRef _Nonnull self, const struct timespec* _Nonnull deadline, const struct timespec* _Nonnull interval, VoidFunc_1 _Nonnull func, void* _Nullable context, uintptr_t tag)
{
    return DispatchQueue_DispatchTimer(self, deadline, interval, (VoidFunc_2)func, context, NULL, 0, 0, tag);
}

// Similar to 'DispatchClosure'. However the function will execute on or after
// 'deadline'. If 'interval' is not 0 or infinity, then the function will execute
// every 'interval' ticks until the timer is removed from the queue.
errno_t DispatchQueue_DispatchTimer(DispatchQueueRef _Nonnull self, const struct timespec* _Nonnull deadline, const struct timespec* _Nonnull interval, VoidFunc_2 _Nonnull func, void* _Nullable context, void* _Nullable args, size_t nArgBytes, uint32_t options, uintptr_t tag)
{
    decl_try_err();
    WorkItem* pItem = NULL;

    if ((options & kDispatchOption_Sync) == kDispatchOption_Sync) {
        return EINVAL;
    }

    if (nArgBytes > MAX_ARG_BYTES) {
        return EINVAL;
    }

    mtx_lock(&self->lock);
    if (self->state >= kQueueState_Terminating) {
        mtx_unlock(&self->lock);
        return ETERMINATED;
    }


    if ((options & kDispatchOption_Coalesce) == kDispatchOption_Coalesce) {
        if (DispatchQueue_HasItemWithTag_Locked(self, tag)) {
            mtx_unlock(&self->lock);
            return EOK;
        }
    }


    try(DispatchQueue_AcquireWorkItem_Locked(self, func, context, args, nArgBytes, tag, &pItem));

    pItem->u.timer.deadline = *deadline;
    pItem->u.timer.interval = *interval;
    pItem->flags |= kWorkItemFlag_Timer;

    if (timespec_gt(interval, &TIMESPEC_ZERO) && !timespec_eq(interval, &TIMESPEC_INF)) {
        pItem->flags |= kWorkItemFlag_IsRepeating;
    }


    WorkItem* pPrevItem = DispatchQueue_AddTimedItem_Locked(self, pItem);
    err = DispatchQueue_AcquireVirtualProcessor_Locked(self);
    if (err != EOK) {
        SList_Remove(&self->timer_queue, &pPrevItem->queue_entry, &pItem->queue_entry);
        throw(err);
    }

    cnd_signal(&self->work_available_signaler);
    mtx_unlock(&self->lock);

    return EOK;

catch:
    if (pItem) {
        DispatchQueue_RelinquishWorkItem_Locked(self, pItem);
    }
    mtx_unlock(&self->lock);

    return err;
}


// Removes all scheduled instances of timers and immediate work items with tag
// 'tag' from the dispatch queue. If the closure of the work item is in the
// process of executing when this function is called then the closure will
// continue to execute uninterrupted. If on the other side, the work item is
// still pending and has not begun executing yet then it will be removed and it
// will not execute.
bool DispatchQueue_RemoveByTag(DispatchQueueRef _Nonnull self, uintptr_t tag)
{
    mtx_lock(&self->lock);
    // Queue termination state isn't relevant here
    const bool r0 = DispatchQueue_RemoveWorkItem_Locked(self, &self->item_queue, tag);
    const bool r1 = DispatchQueue_RemoveWorkItem_Locked(self, &self->timer_queue, tag);
    mtx_unlock(&self->lock);
    return (r0 || r1) ? true : false;
}


// Removes all queued work items, one-shot and repeatable timers from the queue.
void DispatchQueue_Flush(DispatchQueueRef _Nonnull self)
{
    mtx_lock(&self->lock);
    DispatchQueue_Flush_Locked(self);
    mtx_unlock(&self->lock);
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Queue Main Loop
////////////////////////////////////////////////////////////////////////////////

static void _rearm_timer(DispatchQueueRef _Nonnull _Locked self, WorkItem* _Nonnull pItem)
{
    // Repeating timer: rearm it with the next fire date that's in
    // the future (the next fire date we haven't already missed).
    struct timespec now;
    
    clock_gettime(g_mono_clock, &now);
    
    do  {
        timespec_add(&pItem->u.timer.deadline, &pItem->u.timer.interval, &pItem->u.timer.deadline);
    } while (timespec_lt(&pItem->u.timer.deadline, &now));
    
    DispatchQueue_AddTimedItem_Locked(self, pItem);
}

// Wait for work items to arrive or for timers to fire
static WorkItem* _Nullable _get_next_work(DispatchQueueRef _Nonnull _Locked self)
{
    WorkItem* pItem = NULL;
    bool mayRelinquish = false;
    struct timespec now, deadline, dly;

    for (;;) {
        clock_gettime(g_mono_clock, &now);

        // Grab the first timer that's due. We give preference to timers because
        // they are tied to a specific deadline time while immediate work items
        // do not guarantee that they will execute at a specific time. So it's
        // acceptable to push them back on the timeline.
        WorkItem* pFirstTimer = (WorkItem*)self->timer_queue.first;
        if (pFirstTimer && timespec_le(&pFirstTimer->u.timer.deadline, &now)) {
            pItem = (WorkItem*) SList_RemoveFirst(&self->timer_queue);
        }


        // Grab the first work item if no timer is due
        if (pItem == NULL) {
            pItem = (WorkItem*) SList_RemoveFirst(&self->item_queue);
        }


        // We're done if we got an item or we got no item and it is okay to
        // relinquish this VP
        if (pItem) {
            self->items_queued_count--;
            return pItem;
        }

        if (mayRelinquish) {
            return NULL;
        }
            

        // Compute a deadline for the wait. We do not wait if the deadline
        // is equal to the current time or it's in the past
        if (self->timer_queue.first) {
            deadline = ((WorkItem*)self->timer_queue.first)->u.timer.deadline;
        } else {
            timespec_from_sec(&dly, 2);
            timespec_add(&now, &dly, &deadline);
        }


        // Wait for work. This drops the queue lock while we're waiting. This
        // call may return with a ETIMEDOUT error. This is fine. Either some
        // new work has arrived in the meantime or if not then we are free
        // to relinquish the VP since it hasn't done anything useful for a
        // longer time.
        const errno_t err = cnd_timedwait(&self->work_available_signaler, &self->lock, &deadline);
        if (self->state != kQueueState_Running) {
            return NULL;
        }

        if (err == ETIMEDOUT && self->availableConcurrency > self->minConcurrency) {
            mayRelinquish = true;
        }
    }
}

void DispatchQueue_Run(DispatchQueueRef _Nonnull self)
{
    vcpu_t pVP = vcpu_current();
    ConcurrencyLane* pConLane = &self->concurrency_lanes[pVP->dispatchQueueConcurrencyLaneIndex];


    // We hold the lock at all times except:
    // - while waiting for work
    // - while executing a work item
    mtx_lock(&self->lock);

    while (self->state == kQueueState_Running) {
        WorkItem* pItem = _get_next_work(self);

        
        // Relinquish this VP if we did not get an item to execute
        if (pItem == NULL) {
            break;
        }


        // Make the item the active item
        pConLane->active_item = pItem;


        // Drop the lock. We do not want to hold it while the closure is executing
        // and we are (if needed) signaling a completion.
        mtx_unlock(&self->lock);


        // Execute the work item
        pItem->func(pItem->context, pItem->arg);


        // Signal the work item's completion semaphore if needed. Note that we
        // have to set pItem to NULL because the waiter for the sync item will
        // take care of relinquishing it and thus we don't want to relinquish
        // the item down below.
        if ((pItem->flags & kWorkItemFlag_IsSync) == kWorkItemFlag_IsSync) {
            DispatchQueue_SignalWorkItemCompletion(self, pItem, false);
            pItem = NULL;
        }


        // Reacquire the lock
        mtx_lock(&self->lock);


        // Deactivate the item
        pConLane->active_item = NULL;


        if (pItem) {
            // Move the work item back to the item cache if possible or destroy it
            if ((pItem->flags & kWorkItemFlag_IsRepeating) == kWorkItemFlag_IsRepeating && self->state == kQueueState_Running) {
                _rearm_timer(self, pItem);
            }
            else {
                DispatchQueue_RelinquishWorkItem_Locked(self, pItem);
            }
        }
    }

    DispatchQueue_DetachVirtualProcessor_Locked(self, pVP);

    if (self->state >= kQueueState_Terminating) {
        cnd_signal(&self->vp_shutdown_signaler);
    }
    mtx_unlock(&self->lock);
}


class_func_defs(DispatchQueue, Object,
override_func_def(deinit, DispatchQueue, Object)
);
