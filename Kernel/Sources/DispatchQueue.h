//
//  DispatchQueue.h
//  Apollo
//
//  Created by Dietmar Planitzer on 4/21/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef DispatchQueue_h
#define DispatchQueue_h

#include <klib/klib.h>
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

// Sets the cancelled state of the given work item. The work item is marked as
// cancelled if the parameter is true and the cancelled state if cleared if the
// parameter is false. Note that it is the responsibility of the work item
// closure to check the cancelled state and to act appropriately on it.
// Clearing the cancelled state of a work item should normally not be necessary.
// The functionality exists to enable work item caching and reuse.
extern void WorkItem_SetCancelled(WorkItemRef _Nonnull pItem, Bool flag);

// Cancels the given work item. The work item is marked as cancelled but it is
// the responsibility of the work item closure to check the cancelled state and
// to act appropriately on it.
static inline void WorkItem_Cancel(WorkItemRef _Nonnull pItem) {
    WorkItem_SetCancelled(pItem, true);
}

// Returns true if the given work item is in cancelled state.
extern Bool WorkItem_IsCancelled(WorkItemRef _Nonnull pItem);


//
// Timers
//

// Creates a new timer. The timer will fire on or after 'deadline'. If 'interval'
// is greater than 0 then the timer will repeat until cancelled.
extern ErrorCode Timer_Create(TimeInterval deadline, TimeInterval interval, DispatchQueueClosure closure, TimerRef _Nullable * _Nonnull pOutTimer);

extern void Timer_Destroy(TimerRef _Nullable pTimer);

// See WorkItem_SetCancelled() for an explanation.
static inline void Timer_SetCancelled(TimerRef _Nonnull pTimer, Bool flag) {
    WorkItem_SetCancelled((WorkItemRef)pTimer, flag);
}

// Cancels the given timer. The timer is marked as cancelled but it is the
// responsibility of the timer closure to check the cancelled state and to act
// appropriately on it. If the timer is a repeating timer then cancelling it
// stops it from being rescheduled.
static inline void Timer_Cancel(TimerRef _Nonnull pTimer) {
    WorkItem_SetCancelled((WorkItemRef)pTimer, true);
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

// Creates a new dispatch queue. A dispatch queue maintains a list of work items
// and timers and it dispatches those things for execution to a pool of virtual
// processors. Virtual processors are automatically acquired and relinquished
// from the given virtual processor pool as needed.
// A dispatch queue has a minimum, maximum and current concurrency. The minimum
// concurrency is currently always 0, while the maximum concurrency is the
// maximum number of virtual processors that the queue is allowed to acquire and
// maintain at any given time. The current concurrency is the number of virtual
// processors the queue is currently actively maintaining.
// A dispatch queue with a maximum concurrency number of 1 is also knows as a
// serial dispatch queue because all work items and timers are dispatched one
// after the other. No two of them will ever execute in parallel on such a queue.
// A dispatch queue with a maximum concurrency of > 1 is also known as a concurrent
// queue because the queue is able to execute multiple work items and timers in
// parallel.
// The minimum concurrency level should typically be 0. The queue automatically
// acquires virtual processors as needed. However it may make sense to pass a
// number > 0 to this argument to ensure that the queue will always have at least
// this number of virtual processors available. Eg to ensure a certain minimum
// latency from when a work item is scheduled to when it executes.
extern ErrorCode DispatchQueue_Create(Int minConcurrency, Int maxConcurrency, Int qos, Int priority, VirtualProcessorPoolRef _Nonnull vpPoolRef, ProcessRef _Nullable _Weak pProc, DispatchQueueRef _Nullable * _Nonnull pOutQueue);

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
extern void DispatchQueue_Terminate(DispatchQueueRef _Nonnull pQueue);

// Waits until the dispatch queue has reached 'terminated' state which means that
// all VPs have been relinquished.
extern void DispatchQueue_WaitForTerminationCompleted(DispatchQueueRef _Nonnull pQueue);

// Destroys the dispatch queue. The queue is first terminated if it isn't already
// in terminated state. All work items and timers which are still queued up are
// flushed and will not execute anymore. Blocks the caller until the queue has
// been drained, terminated and deallocated.
extern void DispatchQueue_Destroy(DispatchQueueRef _Nullable pQueue);


// Returns the process that owns the dispatch queue. Returns NULL if the dispatch
// queue is not owned by any particular process. Eg the kernel main dispatch queue.
extern ProcessRef _Nullable _Weak DispatchQueue_GetOwningProcess(DispatchQueueRef _Nonnull pQueue);

// Returns the dispatch queue that is associated with the virtual processor that
// is running the calling code. This will always return a dispatch queue for
// callers that are running in a dispatch queue context. It returns NULL though
// for callers that are running on a virtual processor that was directly acquired
// from the virtual processor pool.
extern DispatchQueueRef _Nullable DispatchQueue_GetCurrent(void);


// Synchronously executes the given closure. The closure is executed as soon as
// possible and the caller remains blocked until the closure has finished
// execution. This function returns with an EINTR if the queue is flushed or
// terminated by calling DispatchQueue_Terminate().
extern ErrorCode DispatchQueue_DispatchSync(DispatchQueueRef _Nonnull pQueue, DispatchQueueClosure closure);

// Asynchronously executes the given closure. The closure is executed as soon as
// possible.
extern ErrorCode DispatchQueue_DispatchAsync(DispatchQueueRef _Nonnull pQueue, DispatchQueueClosure closure);

// Asynchronously executes the given closure on or after 'deadline'. The dispatch
// queue will try to execute the closure as close to 'deadline' as possible.
extern ErrorCode DispatchQueue_DispatchAsyncAfter(DispatchQueueRef _Nonnull pQueue, TimeInterval deadline, DispatchQueueClosure closure);


// Synchronously executes the given work item. The work item is executed as
// soon as possible and the caller remains blocked until the work item has
// finished execution. This function returns with an EINTR if the queue is
// flushed or terminated by calling DispatchQueue_Terminate().
// Note that a work item in cancelled state is still dispatched since it is the
// job of the work item closure to check for the cancelled state and to execute
// the appropriate action in this case (eg notify some other code). The cancelled
// state of the work item is not changed, meaning it is not cleared by the
// dispatch function.
extern ErrorCode DispatchQueue_DispatchWorkItemSync(DispatchQueueRef _Nonnull pQueue, WorkItemRef _Nonnull pItem);

// Asynchronously executes the given work item. The work item is executed as
// soon as possible.
extern ErrorCode DispatchQueue_DispatchWorkItemAsync(DispatchQueueRef _Nonnull pQueue, WorkItemRef _Nonnull pItem);


// Asynchronously executes the given timer when it comes due. Note that a timer
// in cancelled state is still dispatched since it is the job of the timer
// closure to check for the cancelled state and to execute the appropriate
// action in this case (eg notify some other code). The cancelled state of the
// timer is not changed, meaning it is not cleared by the dispatch function.
extern ErrorCode DispatchQueue_DispatchTimer(DispatchQueueRef _Nonnull pQueue, TimerRef _Nonnull pTimer);


// Removes all queued work items, one-shot and repeatable timers from the queue.
// Note that queued up DispatchSync() calls will return with an EINTR.
extern void DispatchQueue_Flush(DispatchQueueRef _Nonnull pQueue);
 
#endif /* DispatchQueue_h */
