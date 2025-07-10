//
//  sys/dispatch.h
//  libc
//
//  Created by Dietmar Planitzer on 2/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef _SYS_DISPATCH_H
#define _SYS_DISPATCH_H 1

#include <_cmndef.h>
#include <stdint.h>
#include <time.h>
#include <kpi/dispatch.h>

__CPP_BEGIN

#define kDispatchQueue_Main 0


// Synchronously executes the given closure. The closure is executed as soon as
// possible and the caller remains blocked until the closure has finished
// execution. This function returns with an EINTR if the queue is flushed or
// terminated by calling DispatchQueue_Terminate().
// @Concurrency: Safe
extern int os_dispatch_sync(int od, os_dispatch_func_t _Nonnull func, void* _Nullable context);

// Schedules the given closure for asynchronous execution on the given dispatch
// queue. The 'pContext' argument will be passed to the callback. If the queue
// is a serial queue then the callback will be executed some time after the
// currently executing closure has finished executing. If the queue is a
// concurrent queue then the callback might start executing even while the
// the currently executing closure is still running.
// @Concurrency: Safe 
extern int os_dispatch_async(int od, os_dispatch_func_t _Nonnull func, void* _Nullable context);

// Asynchronously executes the given closure on or after 'deadline'. The dispatch
// queue will try to execute the closure as close to 'deadline' as possible.
// @Concurrency: Safe
extern int os_dispatch_after(int od, const struct timespec* _Nonnull deadline, os_dispatch_func_t _Nonnull func, void* _Nullable context, uintptr_t tag);

// Asynchronously executes the given closure on or after 'deadline'. The dispatch
// queue will try to execute the closure as close to 'deadline' as possible. The
// closure will be executed repeatedly every 'interval' duration until removed.
// @Concurrency: Safe
extern int os_dispatch_periodically(int od, const struct timespec* _Nonnull deadline, const struct timespec* _Nonnull interval, os_dispatch_func_t _Nonnull func, void* _Nullable context, uintptr_t tag);


// Removes all scheduled instances of timers and immediate work items with tag
// 'tag' from the dispatch queue. If the closure of the work item is in the
// process of executing when this function is called then the closure will
// continue to execute uninterrupted. If on the other side, the work item is
// still pending and has not executed yet then it will be removed and it will
// not execute.
extern int os_dispatch_removebytag(int od, uintptr_t tag);


// Returns the dispatch queue that is associated with the virtual processor that
// is running the calling code.
// @Concurrency: Safe
extern int os_dispatch_getcurrent(void);


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
// Returns a dispatch queue descriptor and -1 on failure.
// XXX probably want to gate this somewhat behind a capability to prevent a random
// XXX process from hugging all virtual processors.
// @Concurrency: Safe
extern int os_dispatch_create(int minConcurrency, int maxConcurrency, int qos, int priority);

// Destroys the dispatch queue. The queue is first terminated if it isn't already
// in terminated state. All work items and timers which are still queued up are
// flushed and will not execute anymore. Blocks the caller until the queue has
// been drained, terminated and deallocated. Errors returned from this function
// are purely advisory in nature - they will not stop the queue from being
// destroyed.
// @Concurrency: Safe
extern int os_dispatch_destroy(int od);

__CPP_END

#endif /* _SYS_DISPATCH_H */
