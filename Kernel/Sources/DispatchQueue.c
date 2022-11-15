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
    ListNode                queue_entry;
    DispatchQueue_Closure   closure;
    Byte* _Nullable         context;
    AtomicBool              cancelled;
    Int8                    type;
    Int8                    reserved[2];
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
    WorkItem*           item_cache[MAX_ITEM_CACHE_COUNT];   // Small cache of immediate items
    Int                 item_cache_count;
    Lock                lock;
    ConditionVariable   cond_var;
    Array               vps;            // 'maxConcurrency' virtual processors
    Int8                maxConcurrency;
    Int8                qos;
    Int8                priority;
} DispatchQueue;


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Work Items


static void WorkItem_Init(WorkItem* _Nonnull pItem, enum ItemType type, DispatchQueue_Closure _Nonnull pClosure, Byte* _Nullable pContext)
{
    ListNode_Init(&pItem->queue_entry);
    pItem->closure = pClosure;
    pItem->context = pContext;
    pItem->cancelled = false;
    pItem->type = type;
}

// Creates a work item which will invoke the given closure.
WorkItemRef _Nullable WorkItem_Create(DispatchQueue_Closure _Nonnull pClosure, Byte* _Nullable pContext)
{
    WorkItem* pItem = (WorkItem*) kalloc(sizeof(WorkItem), HEAP_ALLOC_OPTION_CPU);
    
    if (pItem) {
        WorkItem_Init(pItem, kItemType_Immediate, pClosure, pContext);
    }
    return pItem;
}

static void WorkItem_Deinit(WorkItem* _Nonnull pItem)
{
    ListNode_Deinit(&pItem->queue_entry);
    pItem->closure = NULL;
    pItem->context = NULL;
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
TimerRef _Nullable Timer_Create(TimeInterval deadline, TimeInterval interval, DispatchQueue_Closure _Nonnull pClosure, Byte* _Nullable pContext)
{
    enum ItemType type = TimeInterval_Greater(interval, kTimeInterval_Zero) ? kItemType_RepeatingTimer : kItemType_OneShotTimer;
    Timer* pTimer = (Timer*) kalloc(sizeof(Timer), HEAP_ALLOC_OPTION_CPU);
    
    if (pTimer) {
        WorkItem_Init((WorkItem*)pTimer, type, pClosure, pContext);
        pTimer->deadline = deadline;
        pTimer->interval = interval;
    }
    return pTimer;
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


// Called at kernel startup time in order to create all Kernel queues. Aborts
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

static void DispatchQueue_SpawnWorkerIfNeeded_Locked(DispatchQueueRef _Nonnull pQueue, VirtualProcessor_Closure _Nonnull pWorkerFunc)
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

// Dispatches the given work item to the given queue. The work item is executed
// as soon as possible.
void DispatchQueue_DispatchWorkItem(DispatchQueueRef _Nonnull pQueue, WorkItemRef _Nonnull pItem)
{
    Lock_Lock(&pQueue->lock);
    List_InsertAfterLast(&pQueue->item_queue, &pItem->queue_entry);
    DispatchQueue_SpawnWorkerIfNeeded_Locked(pQueue, (VirtualProcessor_Closure)DispatchQueue_Run);
    ConditionVariable_Signal(&pQueue->cond_var, &pQueue->lock);
}

// Adds the given timer to the timer queue. Expects that the queu is already
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

// Dispatches the given timer.
void DispatchQueue_DispatchTimer(DispatchQueueRef _Nonnull pQueue, TimerRef _Nonnull pTimer)
{
    Lock_Lock(&pQueue->lock);
    DispatchQueue_AddTimer_Locked(pQueue, pTimer);
    DispatchQueue_SpawnWorkerIfNeeded_Locked(pQueue, (VirtualProcessor_Closure)DispatchQueue_Run);
    ConditionVariable_Signal(&pQueue->cond_var, &pQueue->lock);
}

// Dispatches the given closure to the given queue. The closure is executed as
// soon as possible.
void DispatchQueue_DispatchAsync(DispatchQueueRef _Nonnull pQueue, DispatchQueue_Closure _Nonnull pClosure, Byte* _Nullable pContext)
{
    WorkItem* pItem = NULL;

    Lock_Lock(&pQueue->lock);
    
    if (pQueue->item_cache_count > 0) {
        for (Int i = 0; i < MAX_ITEM_CACHE_COUNT; i++) {
            if (pQueue->item_cache[i]) {
                pItem = pQueue->item_cache[i];
                WorkItem_Init(pItem, kItemType_Immediate, pClosure, pContext);
                pQueue->item_cache[i] = NULL;
                pQueue->item_cache_count--;
                break;
            }
        }
    } else {
        pItem = WorkItem_Create(pClosure, pContext);
    }
    
    List_InsertAfterLast(&pQueue->item_queue, &pItem->queue_entry);
    DispatchQueue_SpawnWorkerIfNeeded_Locked(pQueue, (VirtualProcessor_Closure)DispatchQueue_Run);
    ConditionVariable_Signal(&pQueue->cond_var, &pQueue->lock);
}

// Dispatches the given closure to the given queue. The closure is executed on
// or as soon as possible after the given deadline.
void DispatchQueue_DispatchAsyncAfter(DispatchQueueRef _Nonnull pQueue, TimeInterval deadline, DispatchQueue_Closure _Nonnull pClosure, Byte* _Nullable pContext)
{
    Timer* pTimer = Timer_Create(deadline, kTimeInterval_Zero, pClosure, pContext);
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
        // iteration and it hasn't been cancelled in the meantime.
        if (pTimerToRearm) {
            if (!pTimerToRearm->item.cancelled) {
                // Repeating timer: rearm it with the next fire date which is
                // in the future (the next fire date we haven't already missed).
                const TimeInterval curTime = MonotonicClock_GetCurrentTime();
                
                do  {
                    pTimerToRearm->deadline = TimeInterval_Add(pTimerToRearm->deadline, pTimerToRearm->interval);
                } while (TimeInterval_Less(pTimerToRearm->deadline, curTime));
                DispatchQueue_AddTimer_Locked(pQueue, pTimerToRearm);
            }

            pTimerToRearm = NULL;
        }
        
        
        // Wait for a work items to arrive or for timers to fire
        while (true) {
            if (pQueue->item_queue.first) {
                break;
            }
            
            // Compute the deadline for the wait. We do not wait if the deadline
            // is now or was in the past
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
        DispatchQueue_Closure pClosure = pItem->closure;
        Byte* pContext = pItem->context;

        switch (pItem->type) {
            case kItemType_Immediate:
                if (!DispatchQueue_MoveWorkItemToCacheIfPossible(pQueue, pItem)) {
                    WorkItem_Destroy(pItem);
                }
                pClosure(pContext);
                break;
                
            case kItemType_OneShotTimer:
                Timer_Destroy((Timer*)pItem);
                pClosure(pContext);
                break;
                
            case kItemType_RepeatingTimer: {
                Timer* pTimer = (Timer*)pItem;
                
                if (pTimer->item.cancelled) {
                    Timer_Destroy(pTimer);
                } else {
                    pTimerToRearm = pTimer;
                }
                pClosure(pContext);
                break;
            }
                
            default:
                abort();
                break;
                
        }
    }
}
