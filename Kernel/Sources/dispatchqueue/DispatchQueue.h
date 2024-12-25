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
#include <System/DispatchQueue.h>

final_class(DispatchQueue, Object);



//
// Dispatch Options
//

// This is a uint32_t which breaks into two parts:
// Lower 16 bits: user space controllable options (defined in libsystem)
// Upper 16 bits: kernel space controllable options (defined here)
enum {
    kDispatchOption_User = 0x10000,     // The provided function should be invoked in user space context rather than kernel context
};
#define kDispatchOptionMask_User    0x0000ffff
#define kDispatchOptionMask_Kernel  0xffff0000


//
// Dispatch Queues
//

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
extern errno_t DispatchQueue_DispatchSync(DispatchQueueRef _Nonnull self, VoidFunc_1 _Nonnull func, void* _Nullable context);

// Same as above but takes additional arguments of 'nArgBytes' size (in bytes).
extern errno_t DispatchQueue_DispatchSyncArgs(DispatchQueueRef _Nonnull self, VoidFunc_2 _Nonnull func, void* _Nullable context, void* _Nullable args, size_t nArgBytes);

// Asynchronously executes the given closure. The closure is executed as soon as
// possible.
extern errno_t DispatchQueue_DispatchAsync(DispatchQueueRef _Nonnull self, VoidFunc_1 _Nonnull func, void* _Nullable context);

// Same as above but takes additional arguments of 'nArgBytes' size (in bytes).
extern errno_t DispatchQueue_DispatchAsyncArgs(DispatchQueueRef _Nonnull self, VoidFunc_2 _Nonnull func, void* _Nullable context, void* _Nullable args, size_t nArgBytes);

// Dispatches 'func' on the dispatch queue. 'func' will be called with 'context'
// as the first argument and a pointer to the arguments as the second argument.
// The argument pointer will be exactly 'args' if 'nArgBytes' is 0 and otherwise
// it will point to a copy of the arguments that 'args' pointed to.
// Use 'options to control whether the function should be executed in kernel or
// user space and whether the caller should be blocked until 'func' has finished
// executing. The function will execute as soon as possible.
extern errno_t DispatchQueue_DispatchClosure(DispatchQueueRef _Nonnull self, VoidFunc_2 _Nonnull func, void* _Nullable context, void* _Nullable args, size_t nArgBytes, uint32_t options, uintptr_t tag);


// Asynchronously executes the given closure on or after 'deadline'. The dispatch
// queue will try to execute the closure as close to 'deadline' as possible. The
// timer can be referenced with the tag 'tag'.
extern errno_t DispatchQueue_DispatchAsyncAfter(DispatchQueueRef _Nonnull self, TimeInterval deadline, VoidFunc_1 _Nonnull func, void* _Nullable context, uintptr_t tag);

// Asynchronously executes the given closure every 'interval' seconds, on or
// after 'deadline' until the timer is removed from the dispatch queue. The
// timer can be referenced with the tag 'tag'.
extern errno_t DispatchQueue_DispatchAsyncPeriodically(DispatchQueueRef _Nonnull self, TimeInterval deadline, TimeInterval interval, VoidFunc_1 _Nonnull func, void* _Nullable context, uintptr_t tag);

// Similar to 'DispatchClosure'. However the function will execute on or after
// 'deadline'. If 'interval' is not 0 or infinity, then the function will execute
// every 'interval' ticks until the timer is removed from the queue.
extern errno_t DispatchQueue_DispatchTimer(DispatchQueueRef _Nonnull self, TimeInterval deadline, TimeInterval interval, VoidFunc_1 _Nonnull func, void* _Nullable context, uint32_t options, uintptr_t tag);


// Removes all scheduled instances of timers and immediate work items with tag
// 'tag' from the dispatch queue. If the closure of the work item is in the
// process of executing when this function is called then the closure will
// continue to execute uninterrupted. If on the other side, the work item is
// still pending and has not executed yet then it will be removed and it will
// not execute.
extern bool DispatchQueue_RemoveByTag(DispatchQueueRef _Nonnull self, uintptr_t tag);


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
