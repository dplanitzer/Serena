//
//  DispatchQueue.c
//  Apollo
//
//  Created by Dietmar Planitzer on 4/21/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "DispatchQueue.h"
#include "Array.h"
#include "ConditionVariable.h"
#include "Heap.h"
#include "List.h"
#include "Lock.h"
#include "MonotonicClock.h"
#include "VirtualProcessorPool.h"
#include "VirtualProcessorScheduler.h"


enum ItemType {
    kItemType_Immediate = 0,    // Execute the item as soon as possible
    kItemType_OneShotTimer,     // Execute the item once on or after its deadline
    kItemType_RepeatingTimer,   // Execute the item on or after its deadline and then reschedule it for the next deadline
};


typedef struct _WorkItem {
    ListNode                            queue_entry;
    DispatchQueue_Closure               closure;
    Byte* _Nullable _Weak               context;
    BinarySemaphore * _Nullable _Weak   completion_sema;
    Bool                                is_owned_by_queue;      // item was created and is owned by the dispatch queue and thus is eligble to be moved to the work item cache
    AtomicBool                          is_being_dispatched;    // shared between all dispatch queues (set to true while the work item is in the process of being dispatched by a queue; false if no queue is using it)
    AtomicBool                          cancelled;              // shared between dispatch queue and queue user
    Int8                                type;
} WorkItem;


typedef struct _Timer {
    WorkItem        item;
    TimeInterval    deadline;           // Time when the timer closure should be executed
    TimeInterval    interval;
} Timer;


#define MAX_ITEM_CACHE_COUNT    4
typedef struct _DispatchQueue {
    List                item_queue;     // Queue of work items that should be executed as soon as possible
    List                timer_queue;    // Queue of items that should be executed on or after their deadline
    WorkItem*           item_cache[MAX_ITEM_CACHE_COUNT];   // Small cache of reusable work items
    Int                 item_cache_count;
    Lock                lock;
    ConditionVariable   cond_var;
    Array               vps;            // 'maxConcurrency' virtual processors providing processing power to the queue
    Int8                maxConcurrency;
    Int8                qos;
    Int8                priority;
} DispatchQueue;


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Work Items


static void WorkItem_Init(WorkItem* _Nonnull pItem, enum ItemType type, Bool isOwnedByQueue, DispatchQueue_Closure _Nonnull pClosure, Byte* _Nullable pContext)
{
    ListNode_Init(&pItem->queue_entry);
    pItem->closure = pClosure;
    pItem->context = pContext;
    pItem->is_owned_by_queue = isOwnedByQueue;
    pItem->is_being_dispatched = false;
    pItem->cancelled = false;
    pItem->type = type;
}

// Creates a work item which will invoke the given closure. Note that work items
// are one-shot: they execute their closure and then the work item is destroyed.
static WorkItemRef _Nullable WorkItem_Create_Internal(DispatchQueue_Closure _Nonnull pClosure, Byte* _Nullable pContext, Bool isOwnedByQueue)
{
    WorkItem* pItem = (WorkItem*) kalloc(sizeof(WorkItem), HEAP_ALLOC_OPTION_CPU);
    
    if (pItem) {
        WorkItem_Init(pItem, kItemType_Immediate, isOwnedByQueue, pClosure, pContext);
    }
    return pItem;
}

// Creates a work item which will invoke the given closure. Note that work items
// are one-shot: they execute their closure and then the work item is destroyed.
// This is the creation method for parties that are external to the dispatch
// queue implementation.
WorkItemRef _Nullable WorkItem_Create(DispatchQueue_Closure _Nonnull pClosure, Byte* _Nullable pContext)
{
    return WorkItem_Create_Internal(pClosure, pContext, false);
}

static void WorkItem_Deinit(WorkItem* _Nonnull pItem)
{
    ListNode_Deinit(&pItem->queue_entry);
    pItem->closure = NULL;
    pItem->context = NULL;
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


// Creates a new timer. The timer will fire on or after 'deadline'. If 'interval'
// is greater than 0 then the timer will repeat until cancelled.
static TimerRef _Nullable Timer_Create_Internal(TimeInterval deadline, TimeInterval interval, DispatchQueue_Closure _Nonnull pClosure, Byte* _Nullable pContext, Bool isOwnedByQueue)
{
    enum ItemType type = TimeInterval_Greater(interval, kTimeInterval_Zero) ? kItemType_RepeatingTimer : kItemType_OneShotTimer;
    Timer* pTimer = (Timer*) kalloc(sizeof(Timer), HEAP_ALLOC_OPTION_CPU);
    
    if (pTimer) {
        WorkItem_Init((WorkItem*)pTimer, type, isOwnedByQueue, pClosure, pContext);
        pTimer->deadline = deadline;
        pTimer->interval = interval;
    }
    return pTimer;
}

// Creates a new timer. The timer will fire on or after 'deadline'. If 'interval'
// is greater than 0 then the timer will repeat until cancelled.
// This is the creation method for parties that are external to the dispatch
// queue implementation.
TimerRef _Nullable Timer_Create(TimeInterval deadline, TimeInterval interval, DispatchQueue_Closure _Nonnull pClosure, Byte* _Nullable pContext)
{
    return Timer_Create_Internal(deadline, interval, pClosure, pContext, false);
}

void Timer_Destroy(TimerRef _Nullable pTimer)
{
    if (pTimer) {
        WorkItem_Deinit((WorkItem *)pTimer);
        kfree((Byte*) pTimer);
    }
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Dispatch Queue


// Called at kernel startup time in order to create all kernel queues. Aborts
// if creation of a queue fails.
void DispatchQueue_CreateKernelQueues(const SystemDescription* _Nonnull pSysDesc)
{
    SystemGlobals* pSysGlobals = SystemGlobals_Get();
    
    pSysGlobals->dispatch_queue_realtime = DispatchQueue_Create(4, DISPATCH_QOS_REALTIME, 0);
    assert(pSysGlobals->dispatch_queue_realtime != NULL);
    
    pSysGlobals->dispatch_queue_main = DispatchQueue_Create(1, DISPATCH_QOS_INTERACTIVE, 0);
    assert(pSysGlobals->dispatch_queue_main != NULL);
    
    pSysGlobals->dispatch_queue_utility = DispatchQueue_Create(4, DISPATCH_QOS_UTILITY, 0);
    assert(pSysGlobals->dispatch_queue_utility != NULL);
    
    pSysGlobals->dispatch_queue_background = DispatchQueue_Create(4, DISPATCH_QOS_BACKGROUND, 0);
    assert(pSysGlobals->dispatch_queue_background != NULL);
    
    pSysGlobals->dispatch_queue_idle = DispatchQueue_Create(2, DISPATCH_QOS_IDLE, 0);
    assert(pSysGlobals->dispatch_queue_idle != NULL);
}

DispatchQueueRef _Nullable DispatchQueue_Create(Int maxConcurrency, Int qos, Int priority)
{
    DispatchQueue* pQueue = (DispatchQueue*) kalloc(sizeof(DispatchQueue), HEAP_ALLOC_OPTION_CPU);
    
    assert(maxConcurrency >= 1);
    
    if (pQueue) {
        List_Init(&pQueue->item_queue);
        List_Init(&pQueue->timer_queue);
        Lock_Init(&pQueue->lock);
        ConditionVariable_Init(&pQueue->cond_var);
        PointerArray_Init(&pQueue->vps, maxConcurrency);

        pQueue->item_cache_count = 0;
        for (Int i = 0; i < MAX_ITEM_CACHE_COUNT; i++) {
            pQueue->item_cache[i] = NULL;
        }

        pQueue->maxConcurrency = (Int8)max(min(maxConcurrency, INT8_MAX), 1);
        pQueue->qos = qos;
        pQueue->priority = priority;
    }
    
    return pQueue;
}

void DispatchQueue_Destroy(DispatchQueueRef _Nullable pQueue)
{
    if (pQueue) {
        // XXX do an orderly shutdown of the VPs somehow
        
        Lock_Lock(&pQueue->lock);
        for (Int i = 0; i < MAX_ITEM_CACHE_COUNT; i++) {
            WorkItem_Destroy(pQueue->item_cache[i]);
            pQueue->item_cache[i] = NULL;
        }
        Lock_Unlock(&pQueue->lock);
        
        List_Deinit(&pQueue->item_queue);
        List_Deinit(&pQueue->timer_queue);
        Lock_Deinit(&pQueue->lock);
        ConditionVariable_Deinit(&pQueue->cond_var);
        PointerArray_Deinit(&pQueue->vps);
        
        kfree((Byte*) pQueue);
    }
}

static void DispatchQueue_SpawnVirtualProcessorIfNeeded_Locked(DispatchQueueRef _Nonnull pQueue, VirtualProcessor_Closure _Nonnull pWorkerFunc)
{
    if (PointerArray_Count(&pQueue->vps) < pQueue->maxConcurrency) {
        VirtualProcessorAttributes attribs;
        
        VirtualProcessorAttributes_Init(&attribs);
        attribs.priority = pQueue->qos * DISPATCH_PRIORITY_COUNT + (pQueue->priority + DISPATCH_PRIORITY_COUNT / 2) + VP_PRIORITIES_RESERVED_LOW;
        VirtualProcessor* pVP = VirtualProcessorPool_AcquireVirtualProcessor(
                                                                            VirtualProcessorPool_GetShared(),
                                                                            &attribs,
                                                                            pWorkerFunc,
                                                                            (Byte*)pQueue,
                                                                            false);
        
        PointerArray_Add(&pQueue->vps, (Byte*)pVP);
        VirtualProcessor_Resume(pVP, false);
    }
}

// Creates a work item for the given closure and closure context. Tries to reuse
// an existing work item from the work item cache whenever possible. Expects that
// the caller holds the dispatch queue lock.
static WorkItem* _Nullable DispatchQueue_CreateWorkItem_Locked(DispatchQueueRef _Nonnull pQueue, DispatchQueue_Closure _Nonnull pClosure, Byte* _Nullable pContext)
{
    WorkItem* pItem = NULL;

    if (pQueue->item_cache_count > 0) {
        for (Int i = 0; i < MAX_ITEM_CACHE_COUNT; i++) {
            if (pQueue->item_cache[i]) {
                pItem = pQueue->item_cache[i];
                WorkItem_Init(pItem, kItemType_Immediate, true, pClosure, pContext);
                pQueue->item_cache[i] = NULL;
                pQueue->item_cache_count--;
                break;
            }
        }
    } else {
        pItem = WorkItem_Create_Internal(pClosure, pContext, true);
    }
    
    return pItem;
}

// Asynchronously executes the given work item. The work item is executed as
// soon as possible. Expects to be called with the dispatch queue held.
static void DispatchQueue_DispatchWorkItemAsync_Locked(DispatchQueueRef _Nonnull pQueue, WorkItemRef _Nonnull pItem)
{
    List_InsertAfterLast(&pQueue->item_queue, &pItem->queue_entry);
    DispatchQueue_SpawnVirtualProcessorIfNeeded_Locked(pQueue, (VirtualProcessor_Closure)DispatchQueue_Run);
    ConditionVariable_Signal(&pQueue->cond_var, &pQueue->lock);
}

// Synchronously executes the given work item. The work item is executed as
// soon as possible and the caller remains blocked until the work item has finished
// execution. Expects that the caller holds the dispatch queue lock.
static void DispatchQueue_DispatchWorkItemSync_Lock(DispatchQueueRef _Nonnull pQueue, WorkItemRef _Nonnull pItem)
{
    BinarySemaphore* pCompletionSema = BinarySemaphore_Create(false);
    assert(pCompletionSema != NULL);

    // Keep in mind that the work item may null out its completion_sema field
    // before BinarySemaphore_Acquire() returns. So we have to keep a local
    // reference to the semaphore around.
    pItem->completion_sema = pCompletionSema;

    DispatchQueue_DispatchWorkItemAsync_Locked(pQueue, pItem);
    BinarySemaphore_Acquire(pCompletionSema, kTimeInterval_Infinity);
    BinarySemaphore_Destroy(pCompletionSema);
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
    DispatchQueue_DispatchWorkItemSync_Lock(pQueue, pItem);
}

// Synchronously executes the given closure. The closure is executed as soon as
// possible and the caller remains blocked until the closure has finished execution.
void DispatchQueue_DispatchSync(DispatchQueueRef _Nonnull pQueue, DispatchQueue_Closure _Nonnull pClosure, Byte* _Nullable pContext)
{
    Lock_Lock(&pQueue->lock);
    WorkItem* pItem = DispatchQueue_CreateWorkItem_Locked(pQueue, pClosure, pContext);
    assert(pItem != NULL);
    DispatchQueue_DispatchWorkItemSync_Lock(pQueue, pItem);
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
    DispatchQueue_DispatchWorkItemAsync_Locked(pQueue, pItem);
}

// Adds the given timer to the timer queue. Expects that the queue is already
// locked. Does not wake up the queue.
static void DispatchQueue_AddTimer_Locked(DispatchQueueRef _Nonnull pQueue, Timer* _Nonnull pTimer)
{
    Timer* pPrevTimer = NULL;
    Timer* pCurTimer = (Timer*)pQueue->timer_queue.first;
    
    while (pCurTimer) {
        if (TimeInterval_Greater(pCurTimer->deadline, pTimer->deadline)) {
            break;
        }
        
        pPrevTimer = pCurTimer;
        pCurTimer = (Timer*)pCurTimer->item.queue_entry.next;
    }
    
    List_InsertAfter(&pQueue->timer_queue, &pTimer->item.queue_entry, &pPrevTimer->item.queue_entry);
}

// Asynchronously executes the given timer when it comes due.
void DispatchQueue_DispatchTimer(DispatchQueueRef _Nonnull pQueue, TimerRef _Nonnull pTimer)
{
    if (AtomicBool_Set(&pTimer->item.is_being_dispatched, true)) {
        // Some other queue is already dispatching this timer
        abort();
    }

    Lock_Lock(&pQueue->lock);
    DispatchQueue_AddTimer_Locked(pQueue, pTimer);
    DispatchQueue_SpawnVirtualProcessorIfNeeded_Locked(pQueue, (VirtualProcessor_Closure)DispatchQueue_Run);
    ConditionVariable_Signal(&pQueue->cond_var, &pQueue->lock);
}

// Asynchronously executes the given closure. The closure is executed as soon as
// possible.
void DispatchQueue_DispatchAsync(DispatchQueueRef _Nonnull pQueue, DispatchQueue_Closure _Nonnull pClosure, Byte* _Nullable pContext)
{
    Lock_Lock(&pQueue->lock);

    WorkItem* pItem = DispatchQueue_CreateWorkItem_Locked(pQueue, pClosure, pContext);
    assert(pItem != NULL);
    
    DispatchQueue_DispatchWorkItemAsync_Locked(pQueue, pItem);
}

// Asynchronously executes the given closure on or after 'deadline'. The dispatch
// queue will try to execute the closure as close to 'deadline' as possible.
void DispatchQueue_DispatchAsyncAfter(DispatchQueueRef _Nonnull pQueue, TimeInterval deadline, DispatchQueue_Closure _Nonnull pClosure, Byte* _Nullable pContext)
{
    Timer* pTimer = Timer_Create_Internal(deadline, kTimeInterval_Zero, pClosure, pContext, true);
    assert(pTimer != NULL);
    
    DispatchQueue_DispatchTimer(pQueue, pTimer);
}

static Bool DispatchQueue_MoveWorkItemToCacheIfPossible(DispatchQueue* _Nonnull pQueue, WorkItem* _Nonnull pItem)
{
    Bool success = false;
    
    Lock_Lock(&pQueue->lock);
    
    if (pQueue->item_cache_count < MAX_ITEM_CACHE_COUNT) {
        for (Int i = 0; i < MAX_ITEM_CACHE_COUNT; i++) {
            if (pQueue->item_cache[i] == NULL) {
                WorkItem_Deinit(pItem);
                pQueue->item_cache[i] = pItem;
                pQueue->item_cache_count++;
                success = true;
                break;
            }
        }
    }
    
    Lock_Unlock(&pQueue->lock);
    return success;
}

void DispatchQueue_Run(DispatchQueueRef _Nonnull pQueue)
{
    WorkItem* pItem = NULL;
    Timer* pTimerToRearm = NULL;

    while (true) {
        pItem = NULL;
        
        Lock_Lock(&pQueue->lock);

        // Rearm a repeating timer if we executed a repeating timer in the previous
        // iteration and it's not been cancelled in the meantime.
        if (pTimerToRearm) {
            if (!pTimerToRearm->item.cancelled) {
                // Repeating timer: rearm it with the next fire date that's in
                // the future (the next fire date we haven't already missed).
                const TimeInterval curTime = MonotonicClock_GetCurrentTime();
                
                do  {
                    pTimerToRearm->deadline = TimeInterval_Add(pTimerToRearm->deadline, pTimerToRearm->interval);
                } while (TimeInterval_Less(pTimerToRearm->deadline, curTime));
                DispatchQueue_AddTimer_Locked(pQueue, pTimerToRearm);
            }

            pTimerToRearm = NULL;
        }
        
        
        // Wait for work items to arrive or for timers to fire
        while (true) {
            if (pQueue->item_queue.first) {
                break;
            }
            
            // Compute a deadline for the wait. We do not wait if the deadline
            // is equal to the current time or it's in the past
            TimeInterval deadline;

            if (pQueue->timer_queue.first) {
                deadline = ((Timer*)pQueue->timer_queue.first)->deadline;
            } else {
                deadline = TimeInterval_Add(MonotonicClock_GetCurrentTime(), TimeInterval_MakeSeconds(2));
            }
            
            
            // Wait for work
            const Int err = ConditionVariable_Wait(&pQueue->cond_var, &pQueue->lock, deadline);
            if (err == ETIMEOUT) {
                break;
            }
        }
        
        
        // Exit this VP if we've waited for work for some time but haven't received
        // any new work
        if (List_IsEmpty(&pQueue->timer_queue) && List_IsEmpty(&pQueue->item_queue)) {
            PointerArray_RemoveIdenticalTo(&pQueue->vps, (Byte*)VirtualProcessor_GetCurrent());
            Lock_Unlock(&pQueue->lock);
            break;
        }

        
        // Grab the first timer that's due
        if (pQueue->timer_queue.first) {
            Timer* pFirstTimer = (Timer*)pQueue->timer_queue.first;
            
            if (TimeInterval_LessEquals(pFirstTimer->deadline, MonotonicClock_GetCurrentTime())) {
                pItem = (WorkItem*)pFirstTimer;
                List_Remove(&pQueue->timer_queue, &pItem->queue_entry);
            }
        }

        
        // Grab the first work item if no timer is due
        if (pItem == NULL && pQueue->item_queue.first) {
            pItem = (WorkItem*)pQueue->item_queue.first;
            List_Remove(&pQueue->item_queue, &pItem->queue_entry);
        }

        Lock_Unlock(&pQueue->lock);


        // Execute the work item
        pItem->closure(pItem->context);


        // Signal the work item's completion semaphore if needed
        if (pItem->completion_sema != NULL) {
            BinarySemaphore_Release(pItem->completion_sema);
            pItem->completion_sema = NULL;
        }


        // Move the work item back to the item cache if possible or destroy it
        switch (pItem->type) {
            case kItemType_Immediate:
                if (pItem->is_owned_by_queue) {
                    if (!DispatchQueue_MoveWorkItemToCacheIfPossible(pQueue, pItem)) {
                        WorkItem_Destroy(pItem);
                    }
                }
                break;
                
            case kItemType_OneShotTimer:
                if (pItem->is_owned_by_queue) {
                    Timer_Destroy((Timer*)pItem);
                }
                break;
                
            case kItemType_RepeatingTimer: {
                Timer* pTimer = (Timer*)pItem;
                
                if (pTimer->item.cancelled && pItem->is_owned_by_queue) {
                    Timer_Destroy(pTimer);
                } else {
                    pTimerToRearm = pTimer;
                }
                break;
            }
                
            default:
                abort();
                break;
        }
    }
}
