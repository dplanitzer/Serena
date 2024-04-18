//
//  DispatchQueuePriv.h
//  kernel
//
//  Created by Dietmar Planitzer on 8/10/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef DispatchQueuePriv_h
#define DispatchQueuePriv_h

#include "DispatchQueue.h"
#include <dispatcher/ConditionVariable.h>
#include <dispatcher/Lock.h>
#include <dispatcher/Semaphore.h>
#include <dispatcher/VirtualProcessorScheduler.h>
#include <driver/MonotonicClock.h>


enum ItemType {
    kItemType_Immediate = 0,    // Execute the item as soon as possible
    kItemType_OneShotTimer,     // Execute the item once on or after its deadline
    kItemType_RepeatingTimer,   // Execute the item on or after its deadline and then reschedule it for the next deadline
};


//
// Completion Signaler
//

// Completion signalers are semaphores that are used to signal the completion of
// a work item to DispatchQueue_DispatchSync()
typedef struct CompletionSignaler {
    SListNode   queue_entry;
    Semaphore   semaphore;
    bool        isInterrupted;
} CompletionSignaler;

extern errno_t CompletionSignaler_Create(CompletionSignaler* _Nullable * _Nonnull pOutComp);
extern void CompletionSignaler_Init(CompletionSignaler* _Nonnull pItem);
extern void CompletionSignaler_Deinit(CompletionSignaler* _Nonnull pItem);
extern void CompletionSignaler_Destroy(CompletionSignaler* _Nullable pItem);


//
// Work Items
//

typedef struct WorkItem {
    SListNode                               queue_entry;
    DispatchQueueClosure                    closure;
    CompletionSignaler * _Nullable _Weak    completion;
    bool                                    is_owned_by_queue;      // item was created and is owned by the dispatch queue and thus is eligble to be moved to the work item cache
    AtomicBool                              is_being_dispatched;    // shared between all dispatch queues (set to true while the work item is in the process of being dispatched by a queue; false if no queue is using it)
    AtomicBool                              cancelled;              // shared between dispatch queue and queue user
    int8_t                                  type;
} WorkItem;

extern errno_t WorkItem_Create_Internal(DispatchQueueClosure closure, bool isOwnedByQueue, WorkItemRef _Nullable * _Nonnull pOutItem);
extern void WorkItem_Init(WorkItemRef _Nonnull pItem, enum ItemType type, DispatchQueueClosure closure, bool isOwnedByQueue);
extern void WorkItem_Deinit(WorkItemRef _Nonnull pItem);
extern void WorkItem_SignalCompletion(WorkItemRef _Nonnull pItem, bool isInterrupted);


//
// Timers
//

typedef struct Timer {
    WorkItem        item;
    TimeInterval    deadline;           // Time when the timer closure should be executed
    TimeInterval    interval;
} Timer;

extern errno_t Timer_Create_Internal(TimeInterval deadline, TimeInterval interval, DispatchQueueClosure closure, bool isOwnedByQueue, TimerRef _Nullable * _Nonnull pOutTimer);
extern void _Nullable Timer_Init(TimerRef _Nonnull pTimer, TimeInterval deadline, TimeInterval interval, DispatchQueueClosure closure, bool isOwnedByQueue);
#define Timer_Deinit(__pTimer) \
    WorkItem_Deinit((WorkItemRef) __pTimer)


//
// Dispatch Queue
//

// A concurrency lane is a virtual processor and all associated resources. The
// resources are specific to this virtual processor and shall only be used in
// connection with this virtual processor. There's one concurrency lane per
// dispatch queue concurrency level.
typedef struct ConcurrencyLane {
    VirtualProcessor* _Nullable  vp;     // The virtual processor assigned to this concurrency lane
} ConcurrencyLane;


enum QueueState {
    kQueueState_Running,                // Queue is running and willing to accept and execute closures
    kQueueState_Terminating,            // DispatchQueue_Terminate() was called and the queue is in the process of terminating
    kQueueState_Terminated              // The queue has finished terminating. All virtual processors are relinquished
};


#define MAX_ITEM_CACHE_COUNT    8
#define MAX_TIMER_CACHE_COUNT   8
#define MAX_COMPLETION_SIGNALER_CACHE_COUNT 8
final_class_ivars(DispatchQueue, Object,
    SList                               item_queue;         // Queue of work items that should be executed as soon as possible
    SList                               timer_queue;        // Queue of items that should be executed on or after their deadline
    SList                               item_cache_queue;   // Cache of reusable work items
    SList                               timer_cache_queue;  // Cache of reusable timers
    SList                               completion_signaler_cache_queue;    // Cache of reusable completion signalers
    Lock                                lock;
    ConditionVariable                   work_available_signaler;    // Used by the queue to indicate to its VPs that a new work item/timer has been enqueued
    ConditionVariable                   vp_shutdown_signaler;       // Used by a VP to indicate that it has relinquished itself because the queue is in the process of shutting down
    ProcessRef _Nullable _Weak          owning_process;             // The process that owns this queue
    int                                 descriptor;                 // The user space descriptor of this queue
    VirtualProcessorPoolRef _Nonnull    virtual_processor_pool;     // Pool from which the queue should retrieve virtual processors
    int                                 items_queued_count;         // Number of work items queued up (item_queue)
    int8_t                              state;                      // The current dispatch queue state
    int8_t                              minConcurrency;             // Minimum number of concurrency lanes that we are required to maintain. So we should not allow availableConcurrency to fall below this when we think we want to voluntarily relinquish a VP
    int8_t                              maxConcurrency;             // Maximum number of concurrency lanes we are allowed to allocate and use
    int8_t                              availableConcurrency;       // Number of concurrency lanes we have acquired and are available for use
    int8_t                              qos;
    int8_t                              priority;
    int8_t                              item_cache_count;
    int8_t                              timer_cache_count;
    int8_t                              completion_signaler_count;
    ConcurrencyLane                     concurrency_lanes[1];       // Up to 'maxConcurrency' concurrency lanes
);

extern void DispatchQueue_deinit(DispatchQueueRef _Nonnull pQueue);

extern void DispatchQueue_Run(DispatchQueueRef _Nonnull pQueue);

static errno_t DispatchQueue_AcquireVirtualProcessor_Locked(DispatchQueueRef _Nonnull pQueue);
static void DispatchQueue_RelinquishWorkItem_Locked(DispatchQueue* _Nonnull pQueue, WorkItemRef _Nonnull pItem);
static void DispatchQueue_RelinquishTimer_Locked(DispatchQueue* _Nonnull pQueue, TimerRef _Nonnull pTimer);

#endif /* DispatchQueuePriv_h */
