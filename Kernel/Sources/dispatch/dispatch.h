//
//  dispatch.h
//  kernel
//
//  Created by Dietmar Planitzer on 7/10/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _DISPATCH_H
#define _DISPATCH_H 1

#include <_cmndef.h>
#include <stdnoreturn.h>
#include <kern/errno.h>
#include <kern/timespec.h>
#include <kern/types.h>
#include <klib/List.h>
#include <kpi/vcpu.h>

__CPP_BEGIN

struct dispatch;
struct dispatch_item;
typedef struct dispatch* dispatch_t;

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
// dispatch_item_t my_item = create_my_item(...);
//
// dispatch_submit(my_dispatcher, my_item);
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
// dispatch_item_t my_item = create_my_item(...)
//
// dispatch_submit(my_dispatcher, DISPATCH_SUBMIT_AWAITABLE, my_item);
// ...
// dispatch_await(my_dispatcher, my_item);
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
typedef void (*dispatch_item_func_t)(struct dispatch_item* _Nonnull item);

// A function which knows how to retire an item that has finished processing or
// was cancelled. Providing this function is optional. The dispatcher will do
// nothing special when retiring an item if the retire function is NULL. Eg it
// won't deallocate the item.
typedef void (*dispatch_retire_func_t)(struct dispatch_item* _Nonnull item);


// Marks an item as awaitable. This means that the item will produce a result
// and that you want to wait for the item to finish its run. You wait for an
// item to finish its execution by calling dispatch_await() on it. The call
// will block until the item has finished processing. It is then safe to
// access the item to retrieve its result. Note that a timer-based item is
// never awaitable.
#define DISPATCH_SUBMIT_AWAITABLE   1

// Specifies that the deadline of a timer-based work item is an absolute time
// value instead of a duration relative to the current time.
#define DISPATCH_SUBMIT_ABSTIME     TIMER_ABSTIME


// The state of an item. Items start out in idle state and transition to pending
// state when they are submitted to a dispatcher. An item transitions to execute
// state when a worker has dequeued it from the work queue. An item finally
// reaches done state after it has finished processing. An item reached the
// cancelled state after it has finished processing and the dispatcher has been
// terminated by calling dispatch_terminate(true).
#define DISPATCH_STATE_IDLE         0
#define DISPATCH_STATE_SCHEDULED    1
#define DISPATCH_STATE_EXECUTING    2
#define DISPATCH_STATE_FINISHED     3
#define DISPATCH_STATE_CANCELLED    4


// The base type of a dispatch item. Embed this structure in your item
// specialization (must be the first field in your structure). You are expected
// to set up the 'func' field and the 'flags' field. All other fields will be
// initialized properly by dispatch_submit().
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
struct dispatch_item {
    SListNode                           qe;
    dispatch_item_func_t _Nonnull       func;
    dispatch_retire_func_t _Nullable    retireFunc;
    uint8_t                             type;
    uint8_t                             subtype;
    uint8_t                             flags;
    volatile int8_t                     state;
};
typedef struct dispatch_item* dispatch_item_t;


// A convenience macro to initialize a dispatch item before it is submitted to
// a dispatcher.
#define DISPATCH_ITEM_INIT(__func, __retireFunc) \
(struct dispatch_item){NULL, (dispatch_item_func_t)(__func), (dispatch_retire_func_t)(__retireFunc), 0, 0, 0, 0}


struct dispatch_timer {
    struct dispatch_item    item;
    struct timespec         deadline;
    struct timespec         interval; 
};
typedef struct dispatch_timer* dispatch_timer_t;


// A convenience macro to initialize a dispatch oneshot timer.
#define DISPATCH_ONESHOT_TIMER(__func, __retireFunc, __deadline) \
(struct dispatch_timer){{NULL, (dispatch_item_func_t)(__func), (dispatch_retire_func_t)(__retireFunc), 0, 0, 0, 0}, (__deadline), TIMESPEC_INF}

// A convenience macro to initialize a dispatch repeating timer.
#define DISPATCH_REPEATING_TIMER(__func, __retireFunc, __deadline, __interval) \
(struct dispatch_timer){{NULL, (dispatch_item_func_t)(__func), (dispatch_retire_func_t)(__retireFunc), 0, 0, 0, 0}, (__deadline), (__interval)}


// Quality of Service level. From highest to lowest.
// DISPATCH_QOS_REALTIME: kernel will minimize the scheduling latency. Realtime is always scheduled before anything else
// DISPATCH_QOS_BACKGROUND: no guarantee with regards to schedule latency.
#define DISPATCH_QOS_REALTIME       SCHED_QOS_REALTIME
#define DISPATCH_QOS_URGENT         SCHED_QOS_URGENT
#define DISPATCH_QOS_INTERACTIVE    SCHED_QOS_INTERACTIVE
#define DISPATCH_QOS_UTILITY        SCHED_QOS_UTILITY
#define DISPATCH_QOS_BACKGROUND     SCHED_QOS_BACKGROUND


// Priorities per QoS level
#define DISPATCH_PRI_HIGHEST    QOS_PRI_HIGHEST
#define DISPATCH_PRI_NORMAL     QOS_PRI_NORMAL
#define DISPATCH_PRI_LOWEST     QOS_PRI_LOWEST


// Information about the current state of concurrency.
typedef struct dispatch_concurrency_info {
    size_t  minimum;    // Minimum required number of workers
    size_t  maximum;    // Maximum allowed number of workers
    size_t  current;    // Number of workers currently assigned to the dispatcher
} dispatch_concurrency_info_t;


#define DISPATCH_MAX_NAME_LENGTH    15

typedef struct dispatch_attr {
    int         version;            // Version 0
    size_t      minConcurrency;
    size_t      maxConcurrency;
    int         qos;
    int         priority;
    const char* _Nullable   name;   // Length limited to DISPATCH_MAX_NAME_LENGTH
} dispatch_attr_t;


// Initializes a dispatch attribute object to set up a serial queue with
// urgent priority.
#define DISPATCH_ATTR_INIT_SERIAL_URGENT(__pri)     (dispatch_attr_t){0, 1, 1, DISPATCH_QOS_URGENT, (__pri), NULL}

// Initializes a dispatch attribute object to set up a concurrent queue with
// '__n' virtual processors and utility priority.
#define DISPATCH_ATTR_INIT_CONCURRENT_UTILITY(__n)  (dispatch_attr_t){0, 1, __n, DISPATCH_QOS_UTILITY, DISPATCH_PRI_NORMAL, NULL}



// Creates a new dispatcher based on the provided dispatcher attributes.
extern errno_t dispatch_create(const dispatch_attr_t* _Nonnull attr, dispatch_t _Nullable * _Nonnull pOutSelf);

// Destroys the given dispatcher. Returns EBUSY if the dispatcher wasn't
// terminated, is still in the process of terminating or there are still
// awaitable items available on which dispatch_await() should be called.
extern errno_t dispatch_destroy(dispatch_t _Nullable self);


// Submits the dispatch item 'item' to the dispatcher 'self'. The item will be
// asynchronously executed as soon as possible. Note that the dispatcher takes
// ownership of 'item' until the item is done processing. Once an item is done
// processing the dispatcher either calls the 'retireFunc' of the item, if the
// item is not awaitable, or it enqueues the item on a result queue if it is
// awaitable. You are required to call dispatch_await() on an awaitable item.
// This will remove the item from the result queue and transfer ownership of the
// item back to you. 'flags' specifies whether the item is awaitable, etc.
extern errno_t dispatch_submit(dispatch_t _Nonnull self, int flags, dispatch_item_t _Nonnull item);

// Waits for the execution of 'item' to finish and removes the item from the
// result queue. Note that this function does not call the 'retireFunc' of the
// item. You are expected to retire the item after dispatch_await() has returned
// and you no longer need access to the result data stored in 'item'. This
// function effectively transfers ownership of 'item' back to you.
extern errno_t dispatch_await(dispatch_t _Nonnull self, dispatch_item_t _Nonnull item);


// Convenience function which creates a simple item to execute a function with
// a single argument asynchronously. The item is created, managed and retired by
// the dispatcher itself. The asynchronous function can not return a result. Use
// dispatch_sync() instead if you need a result back from the function.   
typedef void (*dispatch_async_func_t)(void* _Nullable arg);
extern errno_t dispatch_async(dispatch_t _Nonnull self, dispatch_async_func_t _Nonnull func, void* _Nullable arg);


// A convenience function to synchronously execute a function on the dispatcher
// queue. The function may return an int size value. This value is returned as
// the dispatch_sync() result.
typedef errno_t (*dispatch_sync_func_t)(void* _Nullable arg);
extern errno_t dispatch_sync(dispatch_t _Nonnull self, dispatch_sync_func_t _Nonnull func, void* _Nullable arg);


// Submits the timer 'timer' to the dispatcher 'self'. The timer will execute
// once if the interval field in the timer is set to TIMESPEC_INF and it will
// execute repeatedly if the interval field is set to a valid timespec. The first
// timer execution will be at or soon after 'deadline' and the timer will repeat
// every 'interval' time units. If TIMER_ABSTIME is passed to 'flags' then the
// timer deadline field is interpreted as an absolute time value; otherwise it
// is interpreted as a delay relative to the current time. 
extern errno_t dispatch_timer(dispatch_t _Nonnull self, int flags, dispatch_timer_t _Nonnull timer);


// Convenience function to execute 'func' after 'wtp' nanoseconds or at the
// absolute time 'wtp' if 'flags' contains TIMER_ABSTIME.
extern errno_t dispatch_after(dispatch_t _Nonnull self, int flags, const struct timespec* _Nonnull wtp, dispatch_async_func_t _Nonnull func, void* _Nullable arg);

// Convenience function to repeatedly execute 'func'.
extern errno_t dispatch_repeating(dispatch_t _Nonnull self, int flags, const struct timespec* _Nonnull wtp, const struct timespec* _Nonnull itp, dispatch_async_func_t _Nonnull func, void* _Nullable arg);


// Register the given item as a signal monitor with the dispatcher. The item
// will be scheduled every time the signal is sent to the dispatcher. Remove
// the signal monitor by canceling the item. The dispatcher takes ownership of
// the provided item until it is cancelled. Note that the item will run on one
// worker in a concurrent dispatcher. It will not be invoked multiple times at
// the same time.
extern errno_t dispatch_signal_monitor(dispatch_t _Nonnull self, int signo, dispatch_item_t _Nonnull item);

// Allocates a signal. If 'signo' is <= 0 then the first available USR signal
// with lowest priority is allocated. The signal is allocated in the context of
// the dispatcher 'self'. If 'signo' is a valid signal number in the range of
// SIGUSRMIN to SIGUSRMAX and that signal isn't already allocated in the context
// of the dispatcher, then this signal is marked as allocated. The number of the
// allocated signal is returned on success and -1 otherwise.
// Signals are allocated on a per dispatcher basis since the semantic meaning of
// a signal is potentially different from dispatcher to dispatcher. A signal
// allocated by this function may be passed to some other API so that this API
// can then send the signal to the dispatcher. Use the dispatch_signal_target()
// function to get the vcpu group that should be targeted by the signal.
extern int dispatch_alloc_signal(dispatch_t _Nonnull self, int signo);

// Frees an allocated signal. Does nothing if 'signo' is <= 0 or the signal isn't
// allocated in the first place.
extern void dispatch_free_signal(dispatch_t _Nonnull self, int signo);

// Returns the vcpu group id that should be used in a sigsend() call to send a
// signal to the dispatcher. The signal should be sent with scope
// SIG_SCOPE_VCPU_GROUP.
extern vcpuid_t dispatch_signal_target(dispatch_t _Nonnull self);

// Sends the signal 'signo' to the dispatcher. 'signo' should have been allocated
// with the dispatch_alloc_signal() function. Calling this function is slightly
// preferred over the alternative of calling sigsend() and passing the vcpu
// group id of the dispatcher retrieved by calling dispatch_signal_target(). The
// reason is that this function is able to apply some optimizations to the signal
// sending process that sigsend() can not.
extern errno_t dispatch_send_signal(dispatch_t _Nonnull self, int signo);


// Cancels a scheduled work item or timer and removes it from the dispatcher.
// Note that the work item or timer will finish normally if it is currently
// executing. However it will not get rescheduled anymore. The item is retired
// if it isn't awaitable. It is marked as cancelled and added to the result
// queue if it is awaitable.
extern void dispatch_cancel_item(dispatch_t _Nonnull self, int flags, dispatch_item_t _Nonnull item);

#define DISPATCH_IGNORE_ARG ((void*)-1)

// Cancels the first scheduled timer or work item that matches function 'func'
// and 'arg'. 'arg' is ignored if it is DISPATCH_IGNORE_ARG.
// First here means the timer or work item that would execute soonest. Timers
// are cancelled before work items. At most one timer or work item is cancelled.
extern void dispatch_cancel(dispatch_t _Nonnull self, int flags, dispatch_item_func_t _Nonnull func, void* _Nullable arg);

// Cancels the current item/timer. The current item is the work item that is
// active and belongs to the caller. Does nothing if this function is called
// from outside an item context.
extern void dispatch_cancel_current_item(int flags);

// Returns true if the given item is in cancelled state; false otherwise.
extern bool dispatch_item_cancelled(dispatch_t _Nonnull self, dispatch_item_t _Nonnull item);


// Returns a reference to the current dispatcher. The current dispatcher is the
// dispatcher that manages and owns the vcpu on which the caller is executing.
// Note that this function may return NULL. This happens when the vcpu is not
// owned by any dispatcher. This function will always return a valid dispatcher
// reference if called from inside an item function. The dispatcher in this case
// is the dispatcher on which the item is running.
extern dispatch_t _Nullable dispatch_current_queue(void);

// Retruns a reference to the item that is currently executing on the caller's
// vcpu. This is the item for which the caller is doing work. Note that this
// function returns NULL if it is called from outside an item context. It will
// never return NULL if called from inside am item context.
extern dispatch_item_t _Nullable dispatch_current_item(void);


extern int dispatch_priority(dispatch_t _Nonnull self);
extern errno_t dispatch_setpriority(dispatch_t _Nonnull self, int priority);

extern int dispatch_qos(dispatch_t _Nonnull self);
extern errno_t dispatch_setqos(dispatch_t _Nonnull self, int qos);

extern void dispatch_concurrency_info(dispatch_t _Nonnull self, dispatch_concurrency_info_t* _Nonnull info);

extern errno_t dispatch_name(dispatch_t _Nonnull self, char* _Nonnull buf, size_t buflen);


// Suspends the given dispatcher. Blocks the caller until all workers of the
// dispatcher have reached suspended state. This function may be called more
// than once. Each call increments a suspension count. The dispatcher will be
// resumed once the same number of dispatch_resume() calls have been made as
// dispatch_suspend(). A suspended dispatcher continues to queue work and timer
// requests but it will not process them until it is resumed. Returns 0 on
// success ETERMINATED if the dispatcher is in termination state.
extern errno_t dispatch_suspend(dispatch_t _Nonnull self);

// Resumes the given dispatcher once the suspension count hits 0.
extern void dispatch_resume(dispatch_t _Nonnull self);



#define DISPATCH_TERMINATE_CANCEL_ALL   0x01
#define DISPATCH_TERMINATE_AWAIT_ALL    0x02

// Initiates the termination of a dispatcher. Note that termination is an
// inherently asynchronous operation that may take a while. This function will
// never cancel an item that is currently in processing state. However it may
// cancel items that are still in pending state. Pass the
// DISPATCH_TERMINATE_CANCEL_ALL option in 'flags' if all still pending items
// should be canceled and not executed. Pass 0 if all pending items should be
// allowed to execute before the dispatcher completes termination. Note that the
// dispatcher will no longer accept new items as soon as this function returns.
// Any attempt to submit a new item to the dispatcher will be rejected with an
// ETERMINATED error. Note that you can not terminate the main dispatcher.
// Pass DISPATCH_TERMINATE_AWAIT_ALL in 'flags' to make this call block until
// all still executing work items have finished executing and the dispatcher has
// entered terminated state.
extern void dispatch_terminate(dispatch_t _Nonnull self, int flags);

// Blocks the caller until the dispatcher has completed termination. It is safe
// to call dispatch_destroy() on the dispatcher instance once this function has
// returned.
extern errno_t dispatch_await_termination(dispatch_t _Nonnull self);

__CPP_END

#endif /* _DISPATCH_H */
