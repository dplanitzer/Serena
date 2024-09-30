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

extern void CompletionSignaler_Init(CompletionSignaler* _Nonnull self);
extern void CompletionSignaler_Deinit(CompletionSignaler* _Nonnull self);
extern void CompletionSignaler_Destroy(CompletionSignaler* _Nullable self);


//
// Timers
//

typedef struct Timer {
    SListNode       queue_entry;
    TimeInterval    deadline;           // Time when the timer closure should be executed
    TimeInterval    interval;
    bool            isRepeating;
} Timer;

extern void Timer_Destroy(Timer* _Nullable self);
extern void Timer_Deinit(Timer* _Nonnull self);


//
// Work Items
//

enum {
    kWorkItemOption_IsUser = 1,     // Invoke the work item function in user space
};


typedef struct WorkItem {
    SListNode                               queue_entry;
    Closure1Arg_Func _Nonnull               func;
    void* _Nullable _Weak                   context;
    CompletionSignaler * _Nullable _Weak    completion;
    Timer* _Nullable                        timer;
    uintptr_t                               tag;
    uint8_t                                 options;
} WorkItem;

extern void WorkItem_Destroy(WorkItem* _Nullable self);
extern void WorkItem_Deinit(WorkItem* _Nonnull self);
extern void WorkItem_SignalCompletion(WorkItem* _Nonnull self, bool isInterrupted);


//
// Dispatch Queue
//

// A concurrency lane is a virtual processor and all associated resources. The
// resources are specific to this virtual processor and shall only be used in
// connection with this virtual processor. There's one concurrency lane per
// dispatch queue concurrency level.
typedef struct ConcurrencyLane {
    VirtualProcessor* _Nullable vp;             // The virtual processor assigned to this concurrency lane
    WorkItem* _Nullable         active_item;    // Item currently being executed by the VP
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
    SList                               item_queue;         // SList<WorkItem> Queue of work items that should be executed as soon as possible
    SList                               timer_queue;        // SList<WorkItem> Queue of items that should be executed on or after their deadline
    SList                               item_cache_queue;   // SList<WorkItem> Cache of reusable work items
    SList                               timer_cache_queue;  // SList<Timer> Cache of reusable timers
    SList                               completion_signaler_cache_queue;    // SList<CompletionSignaler> Cache of reusable completion signalers
    Lock                                lock;
    ConditionVariable                   work_available_signaler;    // Used by the queue to indicate to its VPs that a new work item/timer has been enqueued
    ConditionVariable                   vp_shutdown_signaler;       // Used by a VP to indicate that it has relinquished itself because the queue is in the process of shutting down
    ProcessRef _Nullable _Weak          owning_process;             // The process that owns this queue
    int                                 descriptor;                 // The user space descriptor of this queue
    VirtualProcessorPoolRef _Nonnull    virtual_processor_pool;     // Pool from which the queue should retrieve virtual processors
    int                                 items_queued_count;         // Number of work items queued up (item_queue && timer_queue)
    int8_t                              state;                      // The current dispatch queue state
    int8_t                              minConcurrency;             // Minimum number of concurrency lanes that we are required to maintain. So we should not allow availableConcurrency to fall below this when we think we want to voluntarily relinquish a VP
    int8_t                              maxConcurrency;             // Maximum number of concurrency lanes we are allowed to allocate and use
    int8_t                              availableConcurrency;       // Number of concurrency lanes we have acquired and are available for use
    int8_t                              qos;
    int8_t                              priority;
    int8_t                              item_cache_capacity;
    int8_t                              timer_cache_capacity;
    int8_t                              completion_signaler_capacity;
    int8_t                              item_cache_count;
    int8_t                              timer_cache_count;
    int8_t                              completion_signaler_count;
    ConcurrencyLane                     concurrency_lanes[1];       // Up to 'maxConcurrency' concurrency lanes
);

extern void DispatchQueue_deinit(DispatchQueueRef _Nonnull self);

extern void DispatchQueue_Run(DispatchQueueRef _Nonnull self);

static errno_t DispatchQueue_AcquireVirtualProcessor_Locked(DispatchQueueRef _Nonnull self);

static void DispatchQueue_RelinquishWorkItem_Locked(DispatchQueueRef _Nonnull self, WorkItem* _Nonnull pItem);
static void DispatchQueue_RelinquishTimer_Locked(DispatchQueueRef _Nonnull self, Timer* _Nonnull pTimer);
static void DispatchQueue_RelinquishCompletionSignaler_Locked(DispatchQueueRef _Nonnull self, CompletionSignaler* _Nonnull pComp);

static void DispatchQueue_SignalWorkItemCompletion(DispatchQueueRef _Nonnull self, WorkItem* _Nonnull pItem, bool isInterrupted);

#endif /* DispatchQueuePriv_h */
