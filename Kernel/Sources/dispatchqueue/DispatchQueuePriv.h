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
#include <sched/cnd.h>
#include <sched/mtx.h>
#include <sched/sched.h>
#include <sched/sem.h>


//
// Work Items
//

enum {
    kWorkItemFlag_Timer = 1,                // Item is a timer
    kWorkItemFlag_IsRepeating = 2,          // Item is a auto-repeating timer
    kWorkItemFlag_IsSync = 4,               // Set if dispatch() wants to wait for the completion of the item
    kWorkItemFlag_IsInterrupted = 8,        // Set if the item's execution has been interrupted
};


typedef struct Timer {
    struct timespec deadline;           // Time when the timer closure should be executed
    struct timespec interval;
} Timer;


typedef struct WorkItem {
    SListNode           queue_entry;
    VoidFunc_2 _Nonnull func;
    void* _Nullable     context;
    void* _Nullable     arg;
    uintptr_t           tag;
    union {
        Timer       timer;
        sem_t       completionSignaler;
    }                   u;
    uint16_t            args_byte_size;
    uint8_t             flags;
    uint8_t             reserved;
    // __align(ARG_WORD_SIZE)   ARG_WORD    args_area[n]
} WorkItem;

// The work item argument area follows the WorkItem data structure. The argument
// area is aligned to ARG_WORD_SIZE and it is always multiple of ARG_WORD_SIZE
// in size.
#define MAX_ARG_BYTES   256

#if defined(__ILP32__)
#define ARG_WORD_SIZE   4
#elif defined(__LLP64__) || defined(__LP64__)
#define ARG_WORD_SIZE   8
#else
#error "unknown machine model"
#endif


//
// Dispatch Queue
//

// A concurrency lane is a virtual processor and all associated resources. The
// resources are specific to this virtual processor and shall only be used in
// connection with this virtual processor. There's one concurrency lane per
// dispatch queue concurrency level.
typedef struct ConcurrencyLane {
    vcpu_t _Nullable    vp;             // The virtual processor assigned to this concurrency lane
    WorkItem* _Nullable active_item;    // Item currently being executed by the VP
} ConcurrencyLane;


enum QueueState {
    kQueueState_Running,                // Queue is running and willing to accept and execute closures
    kQueueState_Terminating,            // DispatchQueue_Terminate() was called and the queue is in the process of terminating
    kQueueState_Terminated              // The queue has finished terminating. All virtual processors are relinquished
};


#define MAX_ITEM_CACHE_COUNT    8
final_class_ivars(DispatchQueue, Object,
    SList               item_queue;         // SList<WorkItem> Queue of work items that should be executed as soon as possible
    SList               timer_queue;        // SList<WorkItem> Queue of items that should be executed on or after their deadline
    SList               item_cache_queue;   // SList<WorkItem> Cache of reusable work items
    mtx_t               lock;
    cnd_t               work_available_signaler;    // Used by the queue to indicate to its VPs that a new work item/timer has been enqueued
    cnd_t               vp_shutdown_signaler;       // Used by a VP to indicate that it has relinquished itself because the queue is in the process of shutting down
    int                 items_queued_count;         // Number of work items queued up (item_queue && timer_queue)
    volatile int8_t     state;                      // The current dispatch queue state
    int8_t              minConcurrency;             // Minimum number of concurrency lanes that we are required to maintain. So we should not allow availableConcurrency to fall below this when we think we want to voluntarily relinquish a VP
    int8_t              maxConcurrency;             // Maximum number of concurrency lanes we are allowed to allocate and use
    int8_t              availableConcurrency;       // Number of concurrency lanes we have acquired and are available for use
    int8_t              qos;
    int8_t              priority;
    int8_t              item_cache_capacity;
    int8_t              item_cache_count;
    ConcurrencyLane     concurrency_lanes[1];       // Up to 'maxConcurrency' concurrency lanes
);


extern void DispatchQueue_Run(DispatchQueueRef _Nonnull self);

static errno_t DispatchQueue_AcquireVirtualProcessor_Locked(DispatchQueueRef _Nonnull self);

static void DispatchQueue_RelinquishWorkItem_Locked(DispatchQueueRef _Nonnull self, WorkItem* _Nonnull pItem);
static void DispatchQueue_SignalWorkItemCompletion(DispatchQueueRef _Nonnull self, WorkItem* _Nonnull pItem, bool isInterrupted);

#endif /* DispatchQueuePriv_h */
