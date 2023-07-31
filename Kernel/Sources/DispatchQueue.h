//
//  DispatchQueue.h
//  Apollo
//
//  Created by Dietmar Planitzer on 4/21/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef DispatchQueue_h
#define DispatchQueue_h

#include "Foundation.h"
#include "Process.h"
#include "SystemDescription.h"
#include "VirtualProcessorPool.h"


// Quality of Service level. From highest to lowest.
// DISPATCH_QOS_REALTIME: kernel will minimize the scheduling latency. Realtime is always scheduled before anything else
// DISPATCH_QOS_IDLE: no guarantee with regards to schedule latency. Only scheduled if there is nothing to schedule for a DISPATCH_QOS_XXX > DISPATCH_QOS_IDLE
#define DISPATCH_QOS_REALTIME       4
#define DISPATCH_QOS_INTERACTIVE    3
#define DISPATCH_QOS_UTILITY        2
#define DISPATCH_QOS_BACKGROUND     1
#define DISPATCH_QOS_IDLE           0

#define DISPATCH_QOS_COUNT          5


// Priorities per QoS level
#define DISPATCH_PRIORITY_HIGHEST   5
#define DISPATCH_PRIORITY_NORMAL    0
#define DISPATCH_PRIORITY_LOWEST   -6

#define DISPATCH_PRIORITY_COUNT     12


struct _WorkItem;
typedef struct _WorkItem* WorkItemRef;

struct _Timer;
typedef struct _Timer* TimerRef;

struct _DispatchQueue;
typedef struct _DispatchQueue* DispatchQueueRef;



//
// Closures
//

typedef struct _DispatchQueueClosure {
    Closure1Arg_Func _Nonnull   func;
    Byte* _Nullable _Weak       context;
    Bool                        isUser;
    Int8                        reserved[3];
} DispatchQueueClosure;

static inline DispatchQueueClosure DispatchQueueClosure_Make(Closure1Arg_Func _Nonnull pFunc, Byte* _Nullable _Weak pContext) {
    DispatchQueueClosure c;
    c.func = pFunc;
    c.context = pContext;
    c.isUser = false;
    return c;
}

static inline DispatchQueueClosure DispatchQueueClosure_MakeUser(Closure1Arg_Func _Nonnull pFunc, Byte* _Nullable _Weak pContext) {
    DispatchQueueClosure c;
    c.func = pFunc;
    c.context = pContext;
    c.isUser = true;
    return c;
}


//
// Work Items
//


// Creates a work item which will invoke the given closure. Note that work items
// are one-shot: they execute their closure and then the work item is destroyed.
extern ErrorCode WorkItem_Create(DispatchQueueClosure closure, WorkItemRef _Nullable * _Nonnull pOutItem);

// Deallocates the given work item.
extern void WorkItem_Destroy(WorkItemRef _Nullable pItem);

// Cancels the given work item. The work item is marked as cancelled but it is
// the responsibility of the work item closure to check the cancelled state and
// to act appropriately on it.
extern void WorkItem_Cancel(WorkItemRef _Nonnull pItem);

// Returns true if the given work item is in cancelled state.
extern Bool WorkItem_IsCancelled(WorkItemRef _Nonnull pItem);


//
// Timers
//

// Creates a new timer. The timer will fire on or after 'deadline'. If 'interval'
// is greater than 0 then the timer will repeat until cancelled.
extern ErrorCode Timer_Create(TimeInterval deadline, TimeInterval interval, DispatchQueueClosure closure, TimerRef _Nullable * _Nonnull pOutTimer);

extern void Timer_Destroy(TimerRef _Nullable pTimer);

// Cancels the given timer. The timer is marked as cancelled but it is the
// responsibility of the timer closure to check the cancelled state and to act
// appropriately on it. If the timer is a repeating timer then cancelling it
// stops it from being rescheduled.
static inline void Timer_Cancel(TimerRef _Nonnull pTimer) {
    WorkItem_Cancel((WorkItemRef)pTimer);
}

// Returns true if the given timer is in cancelled state.
static inline Bool Timer_IsCancelled(TimerRef _Nonnull pTimer) {
    return WorkItem_IsCancelled((WorkItemRef)pTimer);
}


//
// Dispatch Queues
//

// The kernel main queue. This is a serial queue
extern DispatchQueueRef _Nonnull    gMainDispatchQueue;

extern ErrorCode DispatchQueue_Create(Int maxConcurrency, Int qos, Int priority, VirtualProcessorPoolRef _Nonnull vpPoolRef, ProcessRef _Nullable _Weak pProc, DispatchQueueRef _Nullable * _Nonnull pOutQueue);

// Destroys the dispatch queue after all still pending work items have finished
// executing. Pending one-shot and repeatable timers are cancelled and get no
// more chance to run. Blocks the caller until the queue has been drained and
// deallocated.
extern void DispatchQueue_Destroy(DispatchQueueRef _Nullable pQueue);

// Similar to DispatchQueue_Destroy() but allows you to specify whether still
// pending work items should be flushed or executed.
// \param flush true means that still pending work items are executed before the
//              queue shuts down; false means that all pending work items are
//              flushed out from the queue and not executed
extern void DispatchQueue_DestroyAndFlush(DispatchQueueRef _Nullable pQueue, Bool flush);


// Returns the process that owns the dispatch queue. Returns NULL if the dispatch
// queue is not owned by any particular process. Eg the kernel main dispatch queue.
extern ProcessRef _Nullable _Weak DispatchQueue_GetOwningProcess(DispatchQueueRef _Nonnull pQueue);

// Synchronously executes the given work item. The work item is executed as
// soon as possible and the caller remains blocked until the work item has finished
// execution.
extern ErrorCode DispatchQueue_DispatchWorkItemSync(DispatchQueueRef _Nonnull pQueue, WorkItemRef _Nonnull pItem);

// Synchronously executes the given closure. The closure is executed as soon as
// possible and the caller remains blocked until the closure has finished execution.
extern ErrorCode DispatchQueue_DispatchSync(DispatchQueueRef _Nonnull pQueue, DispatchQueueClosure closure);


// Asynchronously executes the given work item. The work item is executed as
// soon as possible.
extern ErrorCode DispatchQueue_DispatchWorkItemAsync(DispatchQueueRef _Nonnull pQueue, WorkItemRef _Nonnull pItem);

// Asynchronously executes the given closure. The closure is executed as soon as
// possible.
extern ErrorCode DispatchQueue_DispatchAsync(DispatchQueueRef _Nonnull pQueue, DispatchQueueClosure closure);

// Asynchronously executes the given closure on or after 'deadline'. The dispatch
// queue will try to execute the closure as close to 'deadline' as possible.
extern ErrorCode DispatchQueue_DispatchAsyncAfter(DispatchQueueRef _Nonnull pQueue, TimeInterval deadline, DispatchQueueClosure closure);


// Asynchronously executes the given timer when it comes due.
// XXX want to think about the case when this function is called with a cancelled
// XXX timer.
extern ErrorCode DispatchQueue_DispatchTimer(DispatchQueueRef _Nonnull pQueue, TimerRef _Nonnull pTimer);


// Returns the dispatch queue that is associated with the virtual processor that
// is running the calling code. This will always return a dispatch queue for
// callers that are running in a dispatch queue context. It returns NULL though
// for callers that are running on a virtual processor that was directly acquired
// from the virtual processor pool.
extern DispatchQueueRef _Nullable DispatchQueue_GetCurrent(void);


// Removes all queued work items, one-shot and repeatable timers from the queue.
extern ErrorCode DispatchQueue_Flush(DispatchQueueRef _Nonnull pQueue);

#endif /* DispatchQueue_h */
