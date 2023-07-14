//
//  DispatchQueue.c
//  Apollo
//
//  Created by Dietmar Planitzer on 4/21/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "DispatchQueue.h"
#include "ConditionVariable.h"
#include "Heap.h"
#include "List.h"
#include "Lock.h"
#include "MonotonicClock.h"
#include "Semaphore.h"
#include "SystemGlobals.h"
#include "VirtualProcessorPool.h"
#include "VirtualProcessorScheduler.h"


enum ItemType {
    kItemType_Immediate = 0,    // Execute the item as soon as possible
    kItemType_OneShotTimer,     // Execute the item once on or after its deadline
    kItemType_RepeatingTimer,   // Execute the item on or after its deadline and then reschedule it for the next deadline
};


typedef struct _WorkItem {
    SListNode                   queue_entry;
    DispatchQueueClosure        closure;
    Semaphore * _Nullable _Weak completion_sema;
    Bool                        is_owned_by_queue;      // item was created and is owned by the dispatch queue and thus is eligble to be moved to the work item cache
    AtomicBool                  is_being_dispatched;    // shared between all dispatch queues (set to true while the work item is in the process of being dispatched by a queue; false if no queue is using it)
    AtomicBool                  cancelled;              // shared between dispatch queue and queue user
    Int8                        type;
} WorkItem;


typedef struct _Timer {
    WorkItem        item;
    TimeInterval    deadline;           // Time when the timer closure should be executed
    TimeInterval    interval;
} Timer;


// Completion signalers are semaphores that are used to signal the completion of
// a work item to DispatchQueue_DispatchSync()
typedef struct _CompletionSignaler {
    SListNode   queue_entry;
    Semaphore   semaphore;
} CompletionSignaler;


// A concurrency lane is a virtual processor and all associated resources. The
// resources are specific to this virtual processor and shall only be used in
// connection with this virtual processor. There's one concurrency lane per
// dispatch queue concurrency level.
typedef struct _ConcurrencyLane {
    VirtualProcessor* _Nullable  vp;     // The virtual processor assigned to this concurrency lane
} ConcurrencyLane;


enum QueueState {
    kQueueState_Running,                // Queue is running and willing to accept and execute closures
    kQueueState_ShuttingDown            // DispatchQueue_Destroy() was called. The VPs should be relinquished and the queue freed 
};


#define MAX_ITEM_CACHE_COUNT    8
#define MAX_TIMER_CACHE_COUNT   8
#define MAX_COMPLETION_SIGNALER_CACHE_COUNT 8
typedef struct _DispatchQueue {
    SList                       item_queue;         // Queue of work items that should be executed as soon as possible
    SList                       timer_queue;        // Queue of items that should be executed on or after their deadline
    SList                       item_cache_queue;   // Cache of reusable work items
    SList                       timer_cache_queue;  // Cache of reusable timers
    SList                       completion_signaler_cache_queue;    // Cache of reusable completion signalers
    Lock                        lock;
    ConditionVariable           work_available_signaler;    // Used by the queue to indicate to its VPs that a new work item/timer has bbeen enqueued
    ConditionVariable           vp_shutdown_signaler;       // Used by a VP to indicate that it has relinqushed itself because the queue is in the process of shutting down
    ProcessRef _Nullable _Weak  owning_process;             // The process that owns this queue
    Int                         items_queued_count;         // Number of work items queued up (item_queue)
    Int8                        state;                      // The current dispatch queue state
    Int8                        maxConcurrency;             // Maximum number of concurrency lanes we are allowed to allocate and use
    Int8                        availableConcurrency;       // Number of concurrency lanes we have acquired and are available for use
    Int8                        qos;
    Int8                        priority;
    Int8                        item_cache_count;
    Int8                        timer_cache_count;
    Int8                        completion_signaler_count;
    ConcurrencyLane             concurrency_lanes[1];   // Up to 'maxConcurrency' concurrency lanes
} DispatchQueue;


static void DispatchQueue_Run(DispatchQueueRef _Nonnull pQueue);


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Work Items


static void WorkItem_Init(WorkItemRef _Nonnull pItem, enum ItemType type, DispatchQueueClosure closure, Bool isOwnedByQueue)
{
    SListNode_Init(&pItem->queue_entry);
    pItem->closure = closure;
    pItem->is_owned_by_queue = isOwnedByQueue;
    pItem->is_being_dispatched = false;
    pItem->cancelled = false;
    pItem->type = type;
}

// Creates a work item which will invoke the given closure. Note that work items
// are one-shot: they execute their closure and then the work item is destroyed.
static WorkItemRef _Nullable WorkItem_Create_Internal(DispatchQueueClosure closure, Bool isOwnedByQueue)
{
    WorkItemRef pItem = (WorkItemRef) kalloc(sizeof(WorkItem), HEAP_ALLOC_OPTION_CPU);
    
    if (pItem) {
        WorkItem_Init(pItem, kItemType_Immediate, closure, isOwnedByQueue);
    }
    return pItem;
}

// Creates a work item which will invoke the given closure. Note that work items
// are one-shot: they execute their closure and then the work item is destroyed.
// This is the creation method for parties that are external to the dispatch
// queue implementation.
WorkItemRef _Nullable WorkItem_Create(DispatchQueueClosure closure)
{
    return WorkItem_Create_Internal(closure, false);
}

static void WorkItem_Deinit(WorkItemRef _Nonnull pItem)
{
    SListNode_Deinit(&pItem->queue_entry);
    pItem->closure.func = NULL;
    pItem->closure.context = NULL;
    pItem->closure.isUser = false;
    pItem->completion_sema = NULL;
    // Leave is_owned_by_queue alone
    pItem->is_being_dispatched = false;
    pItem->cancelled = false;
}

// Deallocates the given work item.
void WorkItem_Destroy(WorkItemRef _Nullable pItem)
{
    if (pItem) {
        WorkItem_Deinit(pItem);
        kfree((Byte*) pItem);
    }
}

// Cancels the given work item. The work item is marked as cancelled but it is
// the responsibility of the work item closure to check the cancelled state and
// to act appropriately on it.
void WorkItem_Cancel(WorkItemRef _Nonnull pItem)
{
    pItem->cancelled = true;
}

// Returns true if the given work item is in cancelled state.
Bool WorkItem_IsCancelled(WorkItemRef _Nonnull pItem)
{
    return pItem->cancelled;
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Timers


static void _Nullable Timer_Init(TimerRef _Nonnull pTimer, TimeInterval deadline, TimeInterval interval, DispatchQueueClosure closure, Bool isOwnedByQueue)
{
    enum ItemType type = TimeInterval_Greater(interval, kTimeInterval_Zero) ? kItemType_RepeatingTimer : kItemType_OneShotTimer;

    WorkItem_Init((WorkItem*)pTimer, type, closure, isOwnedByQueue);
    pTimer->deadline = deadline;
    pTimer->interval = interval;
}

// Creates a new timer. The timer will fire on or after 'deadline'. If 'interval'
// is greater than 0 then the timer will repeat until cancelled.
static TimerRef _Nullable Timer_Create_Internal(TimeInterval deadline, TimeInterval interval, DispatchQueueClosure closure, Bool isOwnedByQueue)
{
    TimerRef pTimer = (TimerRef) kalloc(sizeof(Timer), HEAP_ALLOC_OPTION_CPU);
    
    if (pTimer) {
        Timer_Init(pTimer, deadline, interval, closure, isOwnedByQueue);
    }
    return pTimer;
}

// Creates a new timer. The timer will fire on or after 'deadline'. If 'interval'
// is greater than 0 then the timer will repeat until cancelled.
// This is the creation method for parties that are external to the dispatch
// queue implementation.
TimerRef _Nullable Timer_Create(TimeInterval deadline, TimeInterval interval, DispatchQueueClosure closure)
{
    return Timer_Create_Internal(deadline, interval, closure, false);
}

static inline void Timer_Deinit(TimerRef pTimer)
{
    WorkItem_Deinit((WorkItemRef) pTimer);
}

void Timer_Destroy(TimerRef _Nullable pTimer)
{
    if (pTimer) {
        Timer_Deinit(pTimer);
        kfree((Byte*) pTimer);
    }
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Completion Signalers


static inline void CompletionSignaler_Init(CompletionSignaler* _Nonnull pItem)
{
    SListNode_Init(&pItem->queue_entry);
}

// Creates a completion signaler.
static CompletionSignaler* _Nullable CompletionSignaler_Create(void)
{
    CompletionSignaler* pItem = (CompletionSignaler*) kalloc(sizeof(CompletionSignaler), HEAP_ALLOC_OPTION_CPU);
    
    if (pItem) {
        CompletionSignaler_Init(pItem);
        Semaphore_Init(&pItem->semaphore, 0);
    }
    return pItem;
}

static inline void CompletionSignaler_Deinit(CompletionSignaler* _Nonnull pItem)
{
    SListNode_Deinit(&pItem->queue_entry);
}

// Deallocates the given completion signaler.
void CompletionSignaler_Destroy(CompletionSignaler* _Nullable pItem)
{
    if (pItem) {
        CompletionSignaler_Deinit(pItem);
        Semaphore_Deinit(&pItem->semaphore);
        kfree((Byte*) pItem);
    }
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Dispatch Queue

// Serial queue
DispatchQueueRef _Nonnull DispatchQueue_GetMain(void)
{
    return SystemGlobals_Get()->kernel_main_dispatch_queue;
}


DispatchQueueRef _Nullable DispatchQueue_Create(Int maxConcurrency, Int qos, Int priority, ProcessRef _Nullable _Weak pProc)
{
    assert(maxConcurrency >= 1);

    DispatchQueueRef pQueue = (DispatchQueueRef) kalloc(sizeof(DispatchQueue) + sizeof(ConcurrencyLane) * (maxConcurrency - 1), HEAP_ALLOC_OPTION_CPU);
        
    if (pQueue) {
        SList_Init(&pQueue->item_queue);
        SList_Init(&pQueue->timer_queue);
        SList_Init(&pQueue->item_cache_queue);
        SList_Init(&pQueue->timer_cache_queue);
        SList_Init(&pQueue->completion_signaler_cache_queue);
        Lock_Init(&pQueue->lock);
        ConditionVariable_Init(&pQueue->work_available_signaler);
        ConditionVariable_Init(&pQueue->vp_shutdown_signaler);
        pQueue->owning_process = pProc;
        pQueue->items_queued_count = 0;
        pQueue->state = kQueueState_Running;
        pQueue->maxConcurrency = (Int8)max(min(maxConcurrency, INT8_MAX), 1);
        pQueue->availableConcurrency = 0;
        pQueue->qos = qos;
        pQueue->priority = priority;
        pQueue->item_cache_count = 0;
        pQueue->timer_cache_count = 0;
        pQueue->completion_signaler_count = 0;

        for (Int i = 0; i < maxConcurrency; i++) {
            pQueue->concurrency_lanes[i].vp = NULL;
        }
    }
    
    return pQueue;
}

// Destroys the dispatch queue after all still pending work items have finished
// executing. Pending one-shot and repeatable timers are cancelled and get no
// more chance to run. Blocks the caller until the queue has been drained and
// deallocated.
void DispatchQueue_Destroy(DispatchQueueRef _Nullable pQueue)
{
    DispatchQueue_DestroyAndFlush(pQueue, false);
}

// Similar to DispatchQueue_Destroy() but allows you to specify whether still
// pending work items should be flushed or executed.
// \param flush true means that still pending work items are executed before the
//              queue shuts down; false means that all pending work items are
//              flushed out from the queue and not executed
void DispatchQueue_DestroyAndFlush(DispatchQueueRef _Nullable pQueue, Bool flush)
{
    if (pQueue) {
        CompletionSignaler* pCompSignaler;
        WorkItemRef pItem;
        TimerRef pTimer;

        // Request queue shutdown. This will stop all dispatch calls from
        // accepting new work items and repeatable timers from rescheduling. This
        // will also cause the VPs to drain the existing work item queue and to
        // relinquish themselves.
        Lock_Lock(&pQueue->lock);
        pQueue->state = kQueueState_ShuttingDown;

        if (flush) {
            // Free the still pending work items now
            while ((pItem = (WorkItemRef) SList_RemoveFirst(&pQueue->item_queue)) != NULL) {
                if (pItem->is_owned_by_queue) {
                    WorkItem_Destroy(pItem);
                }
            }
        }


        // Free the scheduled timers now. We do not run them anymore no matter
        // what because (a) their deadline may be far out in the future but the
        // queue is dying now and (b) because of that code that shuts down a
        // queue can not assume that a scheduled timer may get one last chance
        // to run.
        while ((pTimer = (TimerRef) SList_RemoveFirst(&pQueue->timer_queue)) != NULL) {
            if (pTimer->item.is_owned_by_queue) {
                Timer_Destroy(pTimer);
            }
        }


        // We want to wake _all_ VPs up here since all of them need to relinquish
        // themselves.
        ConditionVariable_Broadcast(&pQueue->work_available_signaler, &pQueue->lock);


        // Wait until all VPs have relinquished and detached themselves from the
        // dispatch queue.
        Lock_Lock(&pQueue->lock);
        while (pQueue->availableConcurrency > 0) {
            ConditionVariable_Wait(&pQueue->vp_shutdown_signaler, &pQueue->lock, kTimeInterval_Infinity);
        }
        Lock_Unlock(&pQueue->lock);


        // No more VPs are attached to this queue. We can now go ahead and free
        // all resources.
        SList_Deinit(&pQueue->item_queue);      // guaranteed to be empty at this point
        SList_Deinit(&pQueue->timer_queue);     // guaranteed to be empty at this point

        while ((pItem = (WorkItemRef) SList_RemoveFirst(&pQueue->item_cache_queue)) != NULL) {
            WorkItem_Destroy(pItem);
        }
        SList_Deinit(&pQueue->item_cache_queue);
        
        while ((pTimer = (TimerRef) SList_RemoveFirst(&pQueue->timer_cache_queue)) != NULL) {
            Timer_Destroy(pTimer);
        }
        SList_Deinit(&pQueue->timer_cache_queue);

        while((pCompSignaler = (CompletionSignaler*) SList_RemoveFirst(&pQueue->completion_signaler_cache_queue)) != NULL) {
            CompletionSignaler_Destroy(pCompSignaler);
        }
        SList_Deinit(&pQueue->completion_signaler_cache_queue);
        
        Lock_Deinit(&pQueue->lock);
        ConditionVariable_Deinit(&pQueue->work_available_signaler);
        ConditionVariable_Deinit(&pQueue->vp_shutdown_signaler);
        pQueue->owning_process = NULL;

        kfree((Byte*) pQueue);
    }
}


// Returns the process that owns the dispatch queue. Returns NULL if the dispatch
// queue is not owned by any particular process. Eg the kernel main dispatch queue.
ProcessRef _Nullable _Weak DispatchQueue_GetOwningProcess(DispatchQueueRef _Nonnull pQueue)
{
    return pQueue->owning_process;
}



// Makes sure that we have enough virtual processors attached to the dispatch queue
// and acquires a virtual processor from the virtual processor pool if necessary.
// The virtual processor is attached to the dispatch queue and remains attached
// until it is relinqushed by the queue.
static void DispatchQueue_AcquireVirtualProcessor_Locked(DispatchQueueRef _Nonnull pQueue, Closure1Arg_Func _Nonnull pWorkerFunc)
{
    // Acquire a new virtual processor if we haven't already filled up all
    // concurrency lanes available to us and one of the following is true:
    // - we don't own a virtual processor at all
    // - we've queued up at least 4 work items
    if (pQueue->availableConcurrency < pQueue->maxConcurrency
        && (pQueue->availableConcurrency == 0 || pQueue->items_queued_count > 4)) {
        Int conLaneIdx = -1;

        for (Int i = 0; i < pQueue->maxConcurrency; i++) {
            if (pQueue->concurrency_lanes[i].vp == NULL) {
                conLaneIdx = i;
                break;
            }
        }
        assert(conLaneIdx != -1);

        const Int priority = pQueue->qos * DISPATCH_PRIORITY_COUNT + (pQueue->priority + DISPATCH_PRIORITY_COUNT / 2) + VP_PRIORITIES_RESERVED_LOW;
        VirtualProcessor* pVP = VirtualProcessorPool_AcquireVirtualProcessor(
                                                                            VirtualProcessorPool_GetShared(),
                                                                            VirtualProcessorParameters_Make(pWorkerFunc, (Byte*)pQueue, VP_DEFAULT_KERNEL_STACK_SIZE, VP_DEFAULT_USER_STACK_SIZE, priority));
        assert(pVP != NULL);

        VirtualProcessor_SetDispatchQueue(pVP, pQueue, conLaneIdx);
        pQueue->concurrency_lanes[conLaneIdx].vp = pVP;
        pQueue->availableConcurrency++;

        VirtualProcessor_Resume(pVP, false);
    }
}

// Relinquishes the given virtual processor. The associated concurrency lane is
// freed up and the virtual processor is returned to the virtual processor pool
// after it has been detached from the dispatch queue. This method should only
// be called right before returning from the Dispatch_Run() method which is the
// method that runs on the virtual processor to execute work items.
static void DispatchQueue_RelinquishVirtualProcessor_Locked(DispatchQueueRef _Nonnull pQueue, VirtualProcessor* _Nonnull pVP)
{
    Int conLaneIdx = pVP->dispatchQueueConcurrenyLaneIndex;

    assert(conLaneIdx >= 0 && conLaneIdx < pQueue->maxConcurrency);

    VirtualProcessor_SetDispatchQueue(pVP, NULL, -1);
    pQueue->concurrency_lanes[conLaneIdx].vp = NULL;
    pQueue->availableConcurrency--;
}

// Creates a work item for the given closure and closure context. Tries to reuse
// an existing work item from the work item cache whenever possible. Expects that
// the caller holds the dispatch queue lock.
static WorkItemRef _Nullable DispatchQueue_AcquireWorkItem_Locked(DispatchQueueRef _Nonnull pQueue, DispatchQueueClosure closure)
{
    WorkItemRef pItem = (WorkItemRef) SList_RemoveFirst(&pQueue->item_cache_queue);

    if (pItem != NULL) {
        WorkItem_Init(pItem, kItemType_Immediate, closure, true);
        pQueue->item_cache_count--;
    } else {
        pItem = WorkItem_Create_Internal(closure, true);
    }
    
    return pItem;
}

// Reqlinquishes the given work item back to the item cache if possible. The
// item is freed if the cache is at capacity. The item must be owned by the
// dispatch queue.
static void DispatchQueue_RelinquishWorkItem_Locked(DispatchQueue* _Nonnull pQueue, WorkItemRef _Nonnull pItem)
{
    assert(pItem->is_owned_by_queue);

    if (pQueue->item_cache_count < MAX_ITEM_CACHE_COUNT) {
        WorkItem_Deinit(pItem);
        SList_InsertBeforeFirst(&pQueue->item_cache_queue, &pItem->queue_entry);
        pQueue->item_cache_count++;
    } else {
        WorkItem_Destroy(pItem);
    }
}

// Creates a timer for the given closure and closure context. Tries to reuse
// an existing timer from the timer cache whenever possible. Expects that the
// caller holds the dispatch queue lock.
static TimerRef _Nullable DispatchQueue_AcquireTimer_Locked(DispatchQueueRef _Nonnull pQueue, TimeInterval deadline, TimeInterval interval, DispatchQueueClosure closure)
{
    TimerRef pTimer = (TimerRef) SList_RemoveFirst(&pQueue->timer_cache_queue);

    if (pTimer != NULL) {
        Timer_Init(pTimer, deadline, interval, closure, true);
        pQueue->timer_cache_count--;
    } else {
        pTimer = Timer_Create_Internal(deadline, interval, closure, true);
    }
    
    return pTimer;
}

// Reqlinquishes the given timer back to the timer cache if possible. The timer
// is freed if the cache is at capacity. The timer must be owned by the dispatch
// queue.
static void DispatchQueue_RelinquishTimer_Locked(DispatchQueue* _Nonnull pQueue, TimerRef _Nonnull pTimer)
{
    assert(pTimer->item.is_owned_by_queue);

    if (pQueue->timer_cache_count < MAX_TIMER_CACHE_COUNT) {
        Timer_Deinit(pTimer);
        SList_InsertBeforeFirst(&pQueue->timer_cache_queue, &pTimer->item.queue_entry);
        pQueue->timer_cache_count++;
    } else {
        Timer_Destroy(pTimer);
    }
}

// Creates a completion signaler. Tries to reusean existing completion signaler
// from the completion signaler cache whenever possible. Expects that
// the caller holds the dispatch queue lock.
static CompletionSignaler* _Nullable DispatchQueue_AcquireCompletionSignaler_Locked(DispatchQueueRef _Nonnull pQueue)
{
    CompletionSignaler* pItem = (CompletionSignaler*) SList_RemoveFirst(&pQueue->completion_signaler_cache_queue);

    if (pItem != NULL) {
        CompletionSignaler_Init(pItem);
        pQueue->completion_signaler_count--;
    } else {
        pItem = CompletionSignaler_Create();
    }
    
    return pItem;
}

// Reqlinquishes the given completion signaler back to the completion signaler
// cache if possible. The completion signaler is freed if the cache is at capacity.
static void DispatchQueue_RelinquishCompletionSignaler_Locked(DispatchQueue* _Nonnull pQueue, CompletionSignaler* _Nonnull pItem)
{
    if (pQueue->completion_signaler_count < MAX_COMPLETION_SIGNALER_CACHE_COUNT) {
        CompletionSignaler_Deinit(pItem);
        SList_InsertBeforeFirst(&pQueue->completion_signaler_cache_queue, &pItem->queue_entry);
        pQueue->completion_signaler_count++;
    } else {
        CompletionSignaler_Destroy(pItem);
    }
}

// Asynchronously executes the given work item. The work item is executed as
// soon as possible. Expects to be called with the dispatch queue held.
static void DispatchQueue_DispatchWorkItemAsync_Locked(DispatchQueueRef _Nonnull pQueue, WorkItemRef _Nonnull pItem)
{
    SList_InsertAfterLast(&pQueue->item_queue, &pItem->queue_entry);
    pQueue->items_queued_count++;

    DispatchQueue_AcquireVirtualProcessor_Locked(pQueue, (Closure1Arg_Func)DispatchQueue_Run);
    ConditionVariable_Signal(&pQueue->work_available_signaler, &pQueue->lock);
}

// Synchronously executes the given work item. The work item is executed as
// soon as possible and the caller remains blocked until the work item has finished
// execution. Expects that the caller holds the dispatch queue lock.
static void DispatchQueue_DispatchWorkItemSync_Locked(DispatchQueueRef _Nonnull pQueue, WorkItemRef _Nonnull pItem)
{
    CompletionSignaler* pCompSignaler = DispatchQueue_AcquireCompletionSignaler_Locked(pQueue);
    Semaphore* pCompSema = &pCompSignaler->semaphore;

    // The work item maintains a weak reference to the cached completion semaphore
    pItem->completion_sema = pCompSema;

    DispatchQueue_DispatchWorkItemAsync_Locked(pQueue, pItem);
    Semaphore_Acquire(pCompSema, kTimeInterval_Infinity);

    Lock_Lock(&pQueue->lock);
    DispatchQueue_RelinquishCompletionSignaler_Locked(pQueue, pCompSignaler);
    Lock_Unlock(&pQueue->lock);
}

// Adds the given timer to the timer queue. Expects that the queue is already
// locked. Does not wake up the queue.
static void DispatchQueue_AddTimer_Locked(DispatchQueueRef _Nonnull pQueue, TimerRef _Nonnull pTimer)
{
    TimerRef pPrevTimer = NULL;
    TimerRef pCurTimer = (TimerRef)pQueue->timer_queue.first;
    
    while (pCurTimer) {
        if (TimeInterval_Greater(pCurTimer->deadline, pTimer->deadline)) {
            break;
        }
        
        pPrevTimer = pCurTimer;
        pCurTimer = (TimerRef)pCurTimer->item.queue_entry.next;
    }
    
    SList_InsertAfter(&pQueue->timer_queue, &pTimer->item.queue_entry, &pPrevTimer->item.queue_entry);
}

// Asynchronously executes the given timer when it comes due. Expects that the
// caller holds the dispatch queue lock.
void DispatchQueue_DispatchTimer_Locked(DispatchQueueRef _Nonnull pQueue, TimerRef _Nonnull pTimer)
{
    DispatchQueue_AddTimer_Locked(pQueue, pTimer);
    DispatchQueue_AcquireVirtualProcessor_Locked(pQueue, (Closure1Arg_Func)DispatchQueue_Run);
    ConditionVariable_Signal(&pQueue->work_available_signaler, &pQueue->lock);
}



// Synchronously executes the given work item. The work item is executed as
// soon as possible and the caller remains blocked until the work item has finished
// execution.
void DispatchQueue_DispatchWorkItemSync(DispatchQueueRef _Nonnull pQueue, WorkItemRef _Nonnull pItem)
{
    if (AtomicBool_Set(&pItem->is_being_dispatched, true)) {
        // Some other queue is already dispatching this work item
        abort();
    }

    Lock_Lock(&pQueue->lock);
    if (pQueue->state >= kQueueState_ShuttingDown) {
        Lock_Unlock(&pQueue->lock);
        return;
    }

    DispatchQueue_DispatchWorkItemSync_Locked(pQueue, pItem);
}

// Synchronously executes the given closure. The closure is executed as soon as
// possible and the caller remains blocked until the closure has finished execution.
void DispatchQueue_DispatchSync(DispatchQueueRef _Nonnull pQueue, DispatchQueueClosure closure)
{
    Lock_Lock(&pQueue->lock);
    if (pQueue->state >= kQueueState_ShuttingDown) {
        Lock_Unlock(&pQueue->lock);
        return;
    }

    WorkItem* pItem = DispatchQueue_AcquireWorkItem_Locked(pQueue, closure);
    assert(pItem != NULL);
    DispatchQueue_DispatchWorkItemSync_Locked(pQueue, pItem);
}


// Asynchronously executes the given work item. The work item is executed as
// soon as possible.
void DispatchQueue_DispatchWorkItemAsync(DispatchQueueRef _Nonnull pQueue, WorkItemRef _Nonnull pItem)
{
    if (AtomicBool_Set(&pItem->is_being_dispatched, true)) {
        // Some other queue is already dispatching this work item
        abort();
    }

    Lock_Lock(&pQueue->lock);
    if (pQueue->state >= kQueueState_ShuttingDown) {
        Lock_Unlock(&pQueue->lock);
        return;
    }

    DispatchQueue_DispatchWorkItemAsync_Locked(pQueue, pItem);
}

// Asynchronously executes the given closure. The closure is executed as soon as
// possible.
void DispatchQueue_DispatchAsync(DispatchQueueRef _Nonnull pQueue, DispatchQueueClosure closure)
{
    Lock_Lock(&pQueue->lock);
    if (pQueue->state >= kQueueState_ShuttingDown) {
        Lock_Unlock(&pQueue->lock);
        return;
    }

    WorkItem* pItem = DispatchQueue_AcquireWorkItem_Locked(pQueue, closure);
    assert(pItem != NULL);
    DispatchQueue_DispatchWorkItemAsync_Locked(pQueue, pItem);
}

// Asynchronously executes the given timer when it comes due.
void DispatchQueue_DispatchTimer(DispatchQueueRef _Nonnull pQueue, TimerRef _Nonnull pTimer)
{
    if (AtomicBool_Set(&pTimer->item.is_being_dispatched, true)) {
        // Some other queue is already dispatching this timer
        abort();
    }

    Lock_Lock(&pQueue->lock);
    if (pQueue->state >= kQueueState_ShuttingDown) {
        Lock_Unlock(&pQueue->lock);
        return;
    }

    DispatchQueue_DispatchTimer_Locked(pQueue, pTimer);
}

// Asynchronously executes the given closure on or after 'deadline'. The dispatch
// queue will try to execute the closure as close to 'deadline' as possible.
void DispatchQueue_DispatchAsyncAfter(DispatchQueueRef _Nonnull pQueue, TimeInterval deadline, DispatchQueueClosure closure)
{
    Lock_Lock(&pQueue->lock);
    if (pQueue->state >= kQueueState_ShuttingDown) {
        Lock_Unlock(&pQueue->lock);
        return;
    }

    Timer* pTimer = DispatchQueue_AcquireTimer_Locked(pQueue, deadline, kTimeInterval_Zero, closure);
    assert(pTimer != NULL);
    DispatchQueue_DispatchTimer_Locked(pQueue, pTimer);
}

// Returns the dispatch queue that is associated with the virtual processor that
// is running the calling code. This will always return a dispatch queue for
// callers that are running in a dispatch queue context. It returns NULL though
// for callers that are running on a virtual processor that was directly acquired
// from the virtual processor pool.
DispatchQueueRef _Nullable DispatchQueue_GetCurrent(void)
{
    return (DispatchQueueRef) VirtualProcessor_GetCurrent()->dispatchQueue;
}

// Removes all queued work items, one-shot and repeatable timers from the queue.
void DispatchQueue_Flush(DispatchQueueRef _Nonnull pQueue)
{
    Lock_Lock(&pQueue->lock);

    // Flush the work item queue
    WorkItemRef pItem;
    while ((pItem = (WorkItemRef) SList_RemoveFirst(&pQueue->item_queue)) != NULL) {
        if (pItem->is_owned_by_queue) {
            DispatchQueue_RelinquishWorkItem_Locked(pQueue, pItem);
        }
    }


    // Flush the timers
    TimerRef pTimer;
    while ((pTimer = (TimerRef) SList_RemoveFirst(&pQueue->timer_queue)) != NULL) {
        if (pTimer->item.is_owned_by_queue) {
            DispatchQueue_RelinquishTimer_Locked(pQueue, pTimer);
        }
    }

    Lock_Unlock(&pQueue->lock);
}



static void DispatchQueue_RearmTimer_Locked(DispatchQueueRef _Nonnull pQueue, TimerRef _Nonnull pTimer)
{
    // Repeating timer: rearm it with the next fire date that's in
    // the future (the next fire date we haven't already missed).
    const TimeInterval curTime = MonotonicClock_GetCurrentTime();
                
    do  {
        pTimer->deadline = TimeInterval_Add(pTimer->deadline, pTimer->interval);
    } while (TimeInterval_Less(pTimer->deadline, curTime));
    
    DispatchQueue_AddTimer_Locked(pQueue, pTimer);
}

static void DispatchQueue_Run(DispatchQueueRef _Nonnull pQueue)
{
    // We hold the lock at all times except:
    // - while waiting for work
    // - while executing a work item
    Lock_Lock(&pQueue->lock);

    while (true) {
        WorkItemRef pItem = NULL;
        Bool mayRelinquish = false;
        
        // Wait for work items to arrive or for timers to fire
        while (true) {
            // Grab the first timer that's due. We give preference to timers because
            // they are tied to a specific deadline time while immediate work items
            // do not guarantee that they will execute at a specific time. So it's
            // acceptable to push them back on the timeline.
            Timer* pFirstTimer = (Timer*)pQueue->timer_queue.first;
            if (pFirstTimer && TimeInterval_LessEquals(pFirstTimer->deadline, MonotonicClock_GetCurrentTime())) {
                pItem = (WorkItemRef) SList_RemoveFirst(&pQueue->timer_queue);
            }


            // Grab the first work item if no timer is due
            if (pItem == NULL) {
                pItem = (WorkItemRef) SList_RemoveFirst(&pQueue->item_queue);
                pQueue->items_queued_count--;
            }



            // We're done with this loop if we got an item to execute or we got
            // no item and we're supposed to shut down
            if (pItem != NULL || (pItem == NULL && (mayRelinquish || pQueue->state >= kQueueState_ShuttingDown))) {
                break;
            }
            

            // Compute a deadline for the wait. We do not wait if the deadline
            // is equal to the current time or it's in the past
            TimeInterval deadline;

            if (pQueue->timer_queue.first) {
                deadline = ((TimerRef)pQueue->timer_queue.first)->deadline;
            } else {
                deadline = TimeInterval_Add(MonotonicClock_GetCurrentTime(), TimeInterval_MakeSeconds(2));
            }


            // Wait for work. This drops the queue lock while we're waiting. This
            // call may return with a ETIMEOUT error. This is fine. Either some
            // new work has arrived in the meantime or if not then we'll relinquish
            // the VP since it hasn't done anything for a long enough time.
            const Int err = ConditionVariable_Wait(&pQueue->work_available_signaler, &pQueue->lock, deadline);
            if (err == ETIMEOUT) {
                mayRelinquish = true;
            }
        }

        
        // Relinquish this VP if we did not get an item to execute
        if (pItem == NULL) {
            break;
        }


        // Drop the lock. We do not want to hold it while the closure is executing
        // and we are (if needed) signaling completion.
        Lock_Unlock(&pQueue->lock);


        // Execute the work item
        if (pItem->closure.isUser) {
            cpu_call_as_user(pItem->closure.func, pItem->closure.context);
        } else {
            pItem->closure.func(pItem->closure.context);
        }

        // Signal the work item's completion semaphore if needed
        if (pItem->completion_sema != NULL) {
            Semaphore_Release(pItem->completion_sema);
            pItem->completion_sema = NULL;
        }


        // Reacquire the lock
        Lock_Lock(&pQueue->lock);


        // Move the work item back to the item cache if possible or destroy it
        switch (pItem->type) {
            case kItemType_Immediate:
                if (pItem->is_owned_by_queue) {
                    DispatchQueue_RelinquishWorkItem_Locked(pQueue, pItem);
                }
                break;
                
            case kItemType_OneShotTimer:
                if (pItem->is_owned_by_queue) {
                    DispatchQueue_RelinquishTimer_Locked(pQueue, (TimerRef) pItem);
                }
                break;
                
            case kItemType_RepeatingTimer: {
                Timer* pTimer = (TimerRef)pItem;
                
                if (pTimer->item.cancelled) {
                    if (pItem->is_owned_by_queue) {
                        DispatchQueue_RelinquishTimer_Locked(pQueue, pTimer);
                    }
                } else if (pQueue->state == kQueueState_Running) {
                    DispatchQueue_RearmTimer_Locked(pQueue, pTimer);
                }
                break;
            }
                
            default:
                abort();
                break;
        }
    }

    DispatchQueue_RelinquishVirtualProcessor_Locked(pQueue, VirtualProcessor_GetCurrent());

    if (pQueue->state >= kQueueState_ShuttingDown) {
        ConditionVariable_Signal(&pQueue->vp_shutdown_signaler, &pQueue->lock);
    } else {
        Lock_Unlock(&pQueue->lock);
    }
}
