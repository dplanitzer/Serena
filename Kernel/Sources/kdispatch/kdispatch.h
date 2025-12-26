//
//  kdispatch.h
//  kernel
//
//  Created by Dietmar Planitzer on 7/10/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _KDISPATCH_H
#define _KDISPATCH_H 1

#include <_cmndef.h>
#include <stdnoreturn.h>
#include <ext/queue.h>
#include <ext/timespec.h>
#include <kern/try.h>
#include <kern/types.h>
#include <kpi/vcpu.h>

__CPP_BEGIN

struct kdispatch;
struct kdispatch_item;
typedef struct kdispatch_item* kdispatch_item_t;
typedef struct kdispatch* kdispatch_t;

// A dispatcher or dispatch queue manages a FIFO queue of work items and
// dispatches those items to a set of virtual processors for execution. A
// serial dispatcher is a dispatcher that operates a single virtual processor
// while a concurrent dispatcher operates at least two virtual processors (and
// potentially many more than two).
//
// A dispatcher may have a fixed number of virtual processors associated with it
// or it may be configured such that it has the freedom to automatically
// relinquish and acquire virtual processors as needed.
//
// There are fundamentally two different ways to dispatch work to a dispatcher.
//
// Work may be dispatched asynchronously. This means that you create a work item
// and that you submit it to the dispatcher without indicating that the item
// should be awaitable. Submitting the item to the dispatcher transfers ownership
// of the item to the dispatcher. The dispatcher executes the item and then
// retires it by calling the retire function of the item once the item is done
// doing its work. There is no way to get back a result from a non-awaitable
// item. This is how you create and dispatch an asynchronous item:
//
// kdispatch_item_t my_item = create_my_item(...);
//
// kdispatch_item_async(my_dispatcher, 0, my_item);
//
// Work may be dispatched synchronously. This means that you create a work item
// mark it as awaitable, submit it, await it and then you retire it to free up
// all resources used by the item.
//
// Dispatching an item synchronously enables you to receive the result of the
// computation that the work item did. However you are fully responsible for
// managing the work item life time. The item will not be automatically retired
// by the dispatcher after the item is done. You have to do that yourself after
// you've awaited the item and retrieved its result.
//
// This is how you create and dispatch a synchronous item:
//
// kdispatch_item_t my_item = create_my_item(...)
//
// kdispatch_item_sync(my_dispatcher, my_item);
//
// Creating an item and dispatching it asynchronously and awaitable is an
// alternative way of doing this. Doing things this way gives you more
// flexibility:
//
// kdispatch_item_t my_item = create_my_item(...);
//
// kdispatch_item_async(my_dispatcher, KDISPATCH_SUBMIT_AWAITABLE, my_item);
// ...
// kdispatch_item_await(my_dispatcher, my_item);
// const int result = my_item->result;
// free_my_item(my_item);
//
// The nature of a work item
//
// A work item represents an invocation and as such observes value semantics.
// The function that is used to execute an item is a reference type - but the
// item itself is a value type. Thus a work item can not be submitted more than
// once and be associated with more than one dispatcher at the same time.


// The function responsible for implementing the work of an item.
typedef void (*kdispatch_item_func_t)(kdispatch_item_t _Nonnull item);

// A function which knows how to retire an item that has finished processing or
// was cancelled. Providing this function is optional. The dispatcher will do
// nothing special when retiring an item if the retire function is NULL. Eg it
// won't deallocate the item.
typedef void (*kdispatch_retire_func_t)(kdispatch_item_t _Nonnull item);


// Marks an item as awaitable. This means that the item will produce a result
// and that you want to wait for the item to finish its run. You wait for an
// item to finish its execution by calling kdispatch_item_await() on it. The
// call will block until the item has finished processing. It is then safe to
// access the item to retrieve its result. Note that a timer-based item is
// never awaitable.
#define KDISPATCH_SUBMIT_AWAITABLE   1

// Specifies that the deadline of a timer-based work item is an absolute time
// value instead of a duration relative to the current time.
#define KDISPATCH_SUBMIT_ABSTIME     TIMER_ABSTIME


// The state of an item. Items start out in idle state and transition to pending
// state when they are submitted to a dispatcher. An item transitions to execute
// state when a worker has dequeued it from the work queue. An item finally
// reaches done state after it has finished processing. An item reached the
// cancelled state after it has finished processing and the dispatcher has been
// terminated by calling kdispatch_terminate(true).
#define KDISPATCH_STATE_IDLE         0
#define KDISPATCH_STATE_SCHEDULED    1
#define KDISPATCH_STATE_EXECUTING    2
#define KDISPATCH_STATE_FINISHED     3
#define KDISPATCH_STATE_CANCELLED    4


// The base type of a dispatch item. Embed this structure in your item
// specialization (must be the first field in your structure). You are expected
// to set up the 'func' and 'retireFunc' fields. All other fields will be
// initialized properly by kdispatch_item_xxx().
//
// Note that a particular item instance can be queued at most once with a
// dispatcher. It is fine to re-submit it once it has completed execution but it
// can not be in pending or executing state more than once at the same time.
// Also note that an item may not be submitted to multiple dispatchers at the
// same time. The reasons are:
// serial queue: submitting the same item multiple times makes no sense since it
//               can only execute once at a time. Just execute it and then re-
//               submit.
// concurrent queue: items have state and having two or more vcpus execute the
//                   same item at the same time would make the state inconsistent.
struct kdispatch_item {
    SListNode                           qe;
    kdispatch_item_func_t _Nonnull      func;
    kdispatch_retire_func_t _Nullable   retireFunc;
    uint8_t                             type;
    uint8_t                             subtype;
    uint8_t                             flags;
    volatile int8_t                     state;
};


// A convenience macro to initialize a dispatch item before it is submitted to
// a dispatcher.
#define KDISPATCH_ITEM_INIT(__func, __retireFunc) \
(struct kdispatch_item){NULL, (kdispatch_item_func_t)(__func), (kdispatch_retire_func_t)(__retireFunc), 0, 0, 0, 0}


// Quality of Service level. From highest to lowest.
// KDISPATCH_QOS_REALTIME: kernel will minimize the scheduling latency. Realtime is always scheduled before anything else
// KDISPATCH_QOS_BACKGROUND: no guarantee with regards to schedule latency.
#define KDISPATCH_QOS_REALTIME       SCHED_QOS_REALTIME
#define KDISPATCH_QOS_URGENT         SCHED_QOS_URGENT
#define KDISPATCH_QOS_INTERACTIVE    SCHED_QOS_INTERACTIVE
#define KDISPATCH_QOS_UTILITY        SCHED_QOS_UTILITY
#define KDISPATCH_QOS_BACKGROUND     SCHED_QOS_BACKGROUND


// Priorities per QoS level
#define KDISPATCH_PRI_HIGHEST    QOS_PRI_HIGHEST
#define KDISPATCH_PRI_NORMAL     QOS_PRI_NORMAL
#define KDISPATCH_PRI_LOWEST     QOS_PRI_LOWEST


// Information about the current state of concurrency.
typedef struct kdispatch_concurrency_info {
    size_t  minimum;    // Minimum required number of workers
    size_t  maximum;    // Maximum allowed number of workers
    size_t  current;    // Number of workers currently assigned to the dispatcher
} kdispatch_concurrency_info_t;


#define KDISPATCH_MAX_NAME_LENGTH    7

typedef struct kdispatch_attr {
    int         version;            // Version 0
    size_t      minConcurrency;
    size_t      maxConcurrency;
    int         qos;
    int         priority;
    const char* _Nullable   name;   // Length limited to KDISPATCH_MAX_NAME_LENGTH
} kdispatch_attr_t;


// Initializes a dispatch attribute object to set up a serial queue with
// urgent priority.
#define KDISPATCH_ATTR_INIT_SERIAL_URGENT(__pri, __name)    (kdispatch_attr_t){0, 1, 1, KDISPATCH_QOS_URGENT, (__pri), __name}

// Initializes a dispatch attribute object to set up a concurrent queue with
// exactly '__n' virtual processors and utility priority. This dispatcher does
// not relinquish unused vcpus. It maintains a fixed set of them.
#define KDISPATCH_ATTR_INIT_FIXED_CONCURRENT_UTILITY(__n, __name)   (dispatch_attr_t){0, __n, __n, KDISPATCH_QOS_UTILITY, KDISPATCH_PRI_NORMAL, __name}

// Initializes a dispatch attribute object to set up a concurrent queue with
// exactly '__n' virtual processors and utility priority. This dispatcher does
// relinquish unused vcpus after some time and reacquires them as needed.
#define KDISPATCH_ATTR_INIT_ELASTIC_CONCURRENT_UTILITY(__n, __name) (dispatch_attr_t){0, 1, __n, KDISPATCH_QOS_UTILITY, KDISPATCH_PRI_NORMAL, NULL, __name}



// Creates a new dispatcher based on the provided dispatcher attributes.
extern errno_t kdispatch_create(const kdispatch_attr_t* _Nonnull attr, kdispatch_t _Nullable * _Nonnull pOutSelf);

// Destroys the given dispatcher. Returns EBUSY if the dispatcher wasn't
// terminated, is still in the process of terminating or there are still
// awaitable items available on which kdispatch_item_await() should be called.
extern errno_t kdispatch_destroy(kdispatch_t _Nullable self);


// Submits the dispatch item 'item' to the dispatcher 'self'. The item will be
// asynchronously executed as soon as possible. Note that the dispatcher takes
// ownership of 'item' until the item is done processing. Once an item is done
// processing the dispatcher either calls the 'retireFunc' of the item, if the
// item is not awaitable, or it enqueues the item on a result queue if it is
// awaitable. You are required to call kdispatch_item_await() on an awaitable
// item. This will remove the item from the result queue and transfer ownership
// of the item back to you. 'flags' specifies whether the item is awaitable, etc.
extern errno_t kdispatch_item_async(kdispatch_t _Nonnull self, int flags, kdispatch_item_t _Nonnull item);

// Similar to kdispatch_item_async(), except that this function blocks until the
// dispatched item has finished execution.
extern errno_t kdispatch_item_sync(kdispatch_t _Nonnull self, kdispatch_item_t _Nonnull item);

// Waits for the execution of 'item' to finish and removes the item from the
// result queue. Note that this function does not call the 'retireFunc' of the
// item. You are expected to retire the item after kdispatch_item_await() has
// returned and you no longer need access to the result data stored in 'item'.
// This function effectively transfers ownership of 'item' back to you.
extern errno_t kdispatch_item_await(kdispatch_t _Nonnull self, kdispatch_item_t _Nonnull item);


// Convenience function which creates a simple item to execute a function with
// a single argument asynchronously. The item is created, managed and retired by
// the dispatcher itself. The asynchronous function can not return a result. Use
// kdispatch_sync() instead if you need a result back from the function.   
typedef void (*kdispatch_async_func_t)(void* _Nullable arg);
extern errno_t kdispatch_async(kdispatch_t _Nonnull self, kdispatch_async_func_t _Nonnull func, void* _Nullable arg);


// A convenience function to synchronously execute a function on the dispatcher
// queue. The function may return an int size value. This value is returned as
// the kdispatch_sync() result.
typedef errno_t (*kdispatch_sync_func_t)(void* _Nullable arg);
extern errno_t kdispatch_sync(kdispatch_t _Nonnull self, kdispatch_sync_func_t _Nonnull func, void* _Nullable arg);


// Dispatches the item to run after a 'wtp' delay or at the absolute time 'wtp'
// if 'flags' has the TIMER_ABSTIME flag set. The dispatcher takes ownership of
// item until that time. Once the item has executed, it's retire function will
// be called and ownership reverts back to the item.
extern errno_t kdispatch_item_after(kdispatch_t _Nonnull self, int flags, const struct timespec* _Nonnull wtp, kdispatch_item_t _Nonnull item);

// Similar to kdispatch_item_after(), except that the item is repeatedly executed
// every 'itp time units until it is canceled.
extern errno_t kdispatch_item_repeating(kdispatch_t _Nonnull self, int flags, const struct timespec* _Nonnull wtp, const struct timespec* _Nonnull itp, kdispatch_item_t _Nonnull item);


// Convenience function to execute 'func' after 'wtp' nanoseconds or at the
// absolute time 'wtp' if 'flags' contains TIMER_ABSTIME.
extern errno_t kdispatch_after(kdispatch_t _Nonnull self, int flags, const struct timespec* _Nonnull wtp, kdispatch_async_func_t _Nonnull func, void* _Nullable arg);

// Convenience function to repeatedly execute 'func'.
extern errno_t kdispatch_repeating(kdispatch_t _Nonnull self, int flags, const struct timespec* _Nonnull wtp, const struct timespec* _Nonnull itp, kdispatch_async_func_t _Nonnull func, void* _Nullable arg);


// Register the given item as a signal monitor with the dispatcher. The item
// will be scheduled every time the signal is sent to the dispatcher. Remove
// the signal monitor by canceling the item. The dispatcher takes ownership of
// the provided item until it is cancelled. Note that the item will run on one
// worker in a concurrent dispatcher. It will not be invoked multiple times at
// the same time.
extern errno_t kdispatch_item_on_signal(kdispatch_t _Nonnull self, int signo, kdispatch_item_t _Nonnull item);

// Allocates a signal. If 'signo' is <= 0 then the first available USR signal
// with lowest priority is allocated. The signal is allocated in the context of
// the dispatcher 'self'. If 'signo' is a valid signal number in the range of
// SIGUSRMIN to SIGUSRMAX and that signal isn't already allocated in the context
// of the dispatcher, then this signal is marked as allocated. The number of the
// allocated signal is returned on success and -1 otherwise.
// Signals are allocated on a per dispatcher basis since the semantic meaning of
// a signal is potentially different from dispatcher to dispatcher. A signal
// allocated by this function may be passed to some other API so that this API
// can then send the signal to the dispatcher. Use the kdispatch_signal_target()
// function to get the vcpu group that should be targeted by the signal.
extern int kdispatch_alloc_signal(kdispatch_t _Nonnull self, int signo);

// Frees an allocated signal. Does nothing if 'signo' is <= 0 or the signal isn't
// allocated in the first place.
extern void kdispatch_free_signal(kdispatch_t _Nonnull self, int signo);

// Returns the vcpu group id that should be used in a sigsend() call to send a
// signal to the dispatcher. The signal should be sent with scope
// SIG_SCOPE_VCPU_GROUP.
extern vcpuid_t kdispatch_signal_target(kdispatch_t _Nonnull self);

// Sends the signal 'signo' to the dispatcher. 'signo' should have been allocated
// with the kdispatch_alloc_signal() function. Calling this function is slightly
// preferred over the alternative of calling sigsend() and passing the vcpu
// group id of the dispatcher retrieved by calling kdispatch_signal_target(). The
// reason is that this function is able to apply some optimizations to the signal
// sending process that sigsend() can not.
extern errno_t kdispatch_send_signal(kdispatch_t _Nonnull self, int signo);


// Cancels a scheduled work item or timer and removes it from the dispatcher.
// Note that the work item or timer will finish normally if it is currently
// executing. However it will not get rescheduled anymore. The item is retired
// if it isn't awaitable. It is marked as cancelled and added to the result
// queue if it is awaitable.
extern void kdispatch_cancel_item(kdispatch_t _Nonnull self, kdispatch_item_t _Nonnull item);

#define KDISPATCH_IGNORE_ARG ((void*)-1)

// Cancels the timer or item with the function 'func' and argument 'arg'. 'arg'
// is only taken into account if it isn't KDISPATCH_IGNORE_ARG. This function
// first checks whether the currently executing item matches the function and
// argument. Then it checks pending timers and finally pending items. The first
// timer or item that matches the provided arguments is cancelled and any
// additional timers/items with matching arguments are not cancelled.
extern void kdispatch_cancel(kdispatch_t _Nonnull self, int flags, kdispatch_item_func_t _Nonnull func, void* _Nullable arg);

// Returns true if the currently executing item is in cancelled state. Expects
// to be called from inside the item function.
extern bool kdispatch_current_item_cancelled(void);

// Returns true if the given item is in cancelled state; false otherwise.
extern bool kdispatch_item_cancelled(kdispatch_t _Nonnull self, kdispatch_item_t _Nonnull item);


// Returns a reference to the current dispatcher. The current dispatcher is the
// dispatcher that manages and owns the vcpu on which the caller is executing.
// Note that this function may return NULL. This happens when the vcpu is not
// owned by any dispatcher. This function will always return a valid dispatcher
// reference if called from inside an item function. The dispatcher in this case
// is the dispatcher on which the item is running.
extern kdispatch_t _Nullable kdispatch_current_queue(void);

// Retruns a reference to the item that is currently executing on the caller's
// vcpu. This is the item for which the caller is doing work. Note that this
// function returns NULL if it is called from outside an item context. It will
// never return NULL if called from inside am item context.
extern kdispatch_item_t _Nullable kdispatch_current_item(void);


extern int kdispatch_priority(kdispatch_t _Nonnull self);
extern errno_t kdispatch_setpriority(kdispatch_t _Nonnull self, int priority);

extern int kdispatch_qos(kdispatch_t _Nonnull self);
extern errno_t kdispatch_setqos(kdispatch_t _Nonnull self, int qos);

extern void kdispatch_concurrency_info(kdispatch_t _Nonnull self, kdispatch_concurrency_info_t* _Nonnull info);

extern errno_t kdispatch_name(kdispatch_t _Nonnull self, char* _Nonnull buf, size_t buflen);


// Suspends the given dispatcher. Blocks the caller until all workers of the
// dispatcher have reached suspended state. This function may be called more
// than once. Each call increments a suspension count. The dispatcher will be
// resumed once the same number of kdispatch_resume() calls have been made as
// kdispatch_suspend(). A suspended dispatcher continues to queue work and timer
// requests but it will not process them until it is resumed. Returns 0 on
// success ETERMINATED if the dispatcher is in termination state.
extern errno_t kdispatch_suspend(kdispatch_t _Nonnull self);

// Resumes the given dispatcher once the suspension count hits 0.
extern void kdispatch_resume(kdispatch_t _Nonnull self);



#define KDISPATCH_TERMINATE_CANCEL_ALL   0x01
#define KDISPATCH_TERMINATE_AWAIT_ALL    0x02

// Initiates the termination of a dispatcher. Note that termination is an
// inherently asynchronous operation that may take a while. This function will
// never cancel an item that is currently in processing state. However it may
// cancel items that are still in pending state. Pass the
// KDISPATCH_TERMINATE_CANCEL_ALL option in 'flags' if all still pending items
// should be canceled and not executed. Pass 0 if all pending items should be
// allowed to execute before the dispatcher completes termination. Note that the
// dispatcher will no longer accept new items as soon as this function returns.
// Any attempt to submit a new item to the dispatcher will be rejected with an
// ETERMINATED error. Note that you can not terminate the main dispatcher.
// Pass KDISPATCH_TERMINATE_AWAIT_ALL in 'flags' to make this call block until
// all still executing work items have finished executing and the dispatcher has
// entered terminated state.
extern void kdispatch_terminate(kdispatch_t _Nonnull self, int flags);

// Blocks the caller until the dispatcher has completed termination. It is safe
// to call kdispatch_destroy() on the dispatcher instance once this function has
// returned.
extern errno_t kdispatch_await_termination(kdispatch_t _Nonnull self);

__CPP_END

#endif /* _KDISPATCH_H */
