//
//  DispatchQueue.h
//  libsystem
//
//  Created by Dietmar Planitzer on 2/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef _SYS_DISPATCH_QUEUE_H
#define _SYS_DISPATCH_QUEUE_H 1

#include <System/_cmndef.h>
#include <System/_nulldef.h>
#include <System/Error.h>
#include <System/TimeInterval.h>
#include <System/Types.h>

__CPP_BEGIN

typedef void (*Dispatch_Closure)(void* _Nullable arg);

#define kDispatchQueue_Main 0


// Quality of Service level. From highest to lowest.
// kDispatchQos_Realtime: kernel will minimize the scheduling latency. Realtime is always scheduled before anything else
// kDispatchQos_Idle: no guarantee with regards to schedule latency. Only scheduled if there is nothing to schedule for a DISPATCH_QOS_XXX > kDispatchQos_Idle
#define kDispatchQos_Realtime       4
#define kDispatchQos_Interactive    3
#define kDispatchQos_Utility        2
#define kDispatchQos_Background     1
#define kDispatchQos_Idle           0

#define kDispatchQos_Count          5


// Priorities per QoS level
#define kDispatchPriority_Highest   5
#define kDispatchPriority_Normal    0
#define kDispatchPriority_Lowest   -6

#define kDispatchPriority_Count     12


#if !defined(__KERNEL__)

// Synchronously executes the given closure. The closure is executed as soon as
// possible and the caller remains blocked until the closure has finished
// execution. This function returns with an EINTR if the queue is flushed or
// terminated by calling DispatchQueue_Terminate().
// @Concurrency: Safe
extern errno_t DispatchQueue_DispatchSync(int od, Dispatch_Closure _Nonnull pClosure, void* _Nullable pContext);

// Schedules the given closure for asynchronous execution on the given dispatch
// queue. The 'pContext' argument will be passed to the callback. If the queue
// is a serial queue then the callback will be executed some time after the
// currently executing closure has finished executing. If the queue is a
// concurrent queue then the callback might start executing even while the
// the currently executing closure is still running.
// @Concurrency: Safe 
extern errno_t DispatchQueue_DispatchAsync(int od, Dispatch_Closure _Nonnull pClosure, void* _Nullable pContext);

// Asynchronously executes the given closure on or after 'deadline'. The dispatch
// queue will try to execute the closure as close to 'deadline' as possible.
// @Concurrency: Safe
extern errno_t DispatchQueue_DispatchAsyncAfter(int od, TimeInterval deadline, Dispatch_Closure _Nonnull pClosure, void* _Nullable pContext);


// Returns the dispatch queue that is associated with the virtual processor that
// is running the calling code.
// @Concurrency: Safe
extern int DispatchQueue_GetCurrent(void);


// Creates a new dispatch queue. A dispatch queue maintains a list of work items
// and timers and it dispatches those things for execution to a pool of virtual
// processors. Virtual processors are automatically acquired and relinquished
// from a system wide virtual processor pool, as needed.
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
// XXX probably want to gate this somewhat behind a capability to prevent a random
// XXX process from hugging all virtual processors.
// @Concurrency: Safe
extern errno_t DispatchQueue_Create(int minConcurrency, int maxConcurrency, int qos, int priority, int* _Nonnull pOutQueue);

// Destroys the dispatch queue. The queue is first terminated if it isn't already
// in terminated state. All work items and timers which are still queued up are
// flushed and will not execute anymore. Blocks the caller until the queue has
// been drained, terminated and deallocated. Errors returned from this function
// are purely advisory in nature - they will not stop the queue from being
// destroyed.
// @Concurrency: Safe
extern errno_t DispatchQueue_Destroy(int od);

#endif /* __KERNEL__ */


// Private
enum {
    kDispatchOption_Sync = 1
};

__CPP_END

#endif /* _SYS_DISPATCH_QUEUE_H */
