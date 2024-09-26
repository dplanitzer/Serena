//
//  DispatchQueue.h
//  kernel
//
//  Created by Dietmar Planitzer on 4/21/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef DispatchQueue_h
#define DispatchQueue_h

#include <klib/klib.h>
#include <kobj/Object.h>
#include <dispatcher/VirtualProcessorPool.h>
#include <hal/SystemDescription.h>
#include <process/Process.h>
#include <System/DispatchQueue.h>

final_class(DispatchQueue, Object);



//
// Closures
//

typedef struct DispatchQueueClosure {
    Closure1Arg_Func _Nonnull   func;
    void* _Nullable _Weak       context;
    bool                        isUser;
    int8_t                      reserved[3];
} DispatchQueueClosure;

#define DispatchQueueClosure_Make(__pFunc, __pContext) \
    ((DispatchQueueClosure) {__pFunc, __pContext, false, {0, 0, 0}})

#define DispatchQueueClosure_MakeUser(__pFunc, __pContext) \
    ((DispatchQueueClosure) {__pFunc, __pContext, true, {0, 0, 0}})


//
// Dispatch Queues
//

// The kernel main queue. This is a serial queue
extern DispatchQueueRef _Nonnull    gMainDispatchQueue;

// Creates a new dispatch queue. A dispatch queue maintains a list of work items
// and timers and it dispatches those things for execution to a pool of virtual
// processors. Virtual processors are automatically acquired and relinquished
// from the given virtual processor pool, as needed.
// A dispatch queue has a minimum, maximum and current concurrency. The minimum
// concurrency is currently always 0, while the maximum concurrency is the
// maximum number of virtual processors that the queue is allowed to acquire and
// maintain at any given time. The current concurrency is the number of virtual
// processors the queue is currently actively maintaining.
// A dispatch queue with a maximum concurrency number of 1 is also known as a
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
extern errno_t DispatchQueue_Create(int minConcurrency, int maxConcurrency, int qos, int priority, VirtualProcessorPoolRef _Nonnull vpPoolRef, ProcessRef _Nullable _Weak pProc, DispatchQueueRef _Nullable * _Nonnull pOutQueue);

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
extern void DispatchQueue_Terminate(DispatchQueueRef _Nonnull self);

// Waits until the dispatch queue has reached 'terminated' state which means that
// all VPs have been relinquished.
extern void DispatchQueue_WaitForTerminationCompleted(DispatchQueueRef _Nonnull self);

// Destroys the dispatch queue. The queue is first terminated if it isn't already
// in terminated state. All work items and timers which are still queued up are
// flushed and will not execute anymore. Blocks the caller until the queue has
// been drained, terminated and deallocated.
// Object_Release()


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
extern errno_t DispatchQueue_DispatchSync(DispatchQueueRef _Nonnull self, DispatchQueueClosure closure);

// Asynchronously executes the given closure. The closure is executed as soon as
// possible.
extern errno_t DispatchQueue_DispatchAsync(DispatchQueueRef _Nonnull self, DispatchQueueClosure closure);

// Asynchronously executes the given closure on or after 'deadline'. The dispatch
// queue will try to execute the closure as close to 'deadline' as possible. The
// timer can be referenced with the tag 'tag'.
extern errno_t DispatchQueue_DispatchAsyncAfter(DispatchQueueRef _Nonnull self, TimeInterval deadline, DispatchQueueClosure closure, uintptr_t tag);

// Asynchronously executes the given closure every 'interval' seconds, on or
// after 'deadline' until the timer is removed from the dispatch queue. The
// timer can be referenced with the tag 'tag'.
extern errno_t DispatchQueue_DispatchAsyncPeriodically(DispatchQueueRef _Nonnull self, TimeInterval deadline, TimeInterval interval, DispatchQueueClosure closure, uintptr_t tag);


// Removes all scheduled instances of timers with tag 'tag' from the dispatch
// queue. If the closure of the timer is in the process of executing when this
// function is called then the closure will continue to execute uninterrupted.
// If on the other side, the timer is still pending and has not executed yet
// then it will be removed and it will not execute.
// Returns true if a timer was removed; false otherwise.
extern bool DispatchQueue_RemoveTimer(DispatchQueueRef _Nonnull self, uintptr_t tag);


// Removes all queued work items, one-shot and repeatable timers from the queue.
// Note that queued up DispatchSync() calls will return with an EINTR.
extern void DispatchQueue_Flush(DispatchQueueRef _Nonnull self);


// Returns the process that owns the dispatch queue. Returns NULL if the dispatch
// queue is not owned by any particular process. Eg the kernel main dispatch queue.
extern ProcessRef _Nullable _Weak DispatchQueue_GetOwningProcess(DispatchQueueRef _Nonnull self);

// Sets the dispatch queue descriptor
// Not concurrency safe
extern void DispatchQueue_SetDescriptor(DispatchQueueRef _Nonnull self, int desc);

// Returns the dispatch queue descriptor and -1 if no descriptor has been set on
// the queue.
// Not concurrency safe
extern int DispatchQueue_GetDescriptor(DispatchQueueRef _Nonnull self);

#endif /* DispatchQueue_h */
