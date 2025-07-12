//
//  dispatch.h
//  libdispatch
//
//  Created by Dietmar Planitzer on 7/10/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _DISPATCH_H
#define _DISPATCH_H 1

#include <_cmndef.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/queue.h>

__CPP_BEGIN

struct dispatch;
struct dispatch_item;
typedef struct dispatch* dispatch_t;


// The function responsible for implementing the work of an item.
typedef void (*dispatch_item_func_t)(struct dispatch_item* _Nonnull item);

// A function which knows how to retire an item that has finished processing or
// was cancelled. Providing this function is optional. The item will be
// deallocated by calling free() if no retire function is provided.
typedef void (*dispatch_retire_func_t)(struct dispatch_item* _Nonnull item);


// Marks an item as joinable. This means that the item is able to produce and
// deliver a result. You have to call dispatch_join() on a joinable item. The
// call will block until the item has finished processing. It is then safe to
// access the item to retrieve its result.
#define DISPATCH_ITEM_JOINABLE  1


// The state of an item. Items start out in idle state and transition to pending
// state when they are submitted to a dispatcher. An item transitions to execute
// state when a worker has dequeued it from the work queue. An item finally
// reaches done state after it has finished processing. An item reached the
// cancelled state after it has finished processing and the dispatcher has been
// terminated by calling dispatch_terminate(true).
#define DISPATCH_STATE_IDLE         0
#define DISPATCH_STATE_PENDING      1
#define DISPATCH_STATE_EXECUTING    2
#define DISPATCH_STATE_DONE         3
#define DISPATCH_STATE_CANCELLED    4


// The base type of a dispatch item. Embed this structure in your item
// specialization (must be the first field in your structure). You are expected
// to set up the 'itemFunc' field and the 'flags' field. All other fields will
// be initialized properly by dispatch_submit().
struct dispatch_item {
    SListNode                           qe;
    dispatch_item_func_t _Nonnull       itemFunc;
    dispatch_retire_func_t _Nullable    retireFunc;
    uint16_t                            flags;
    volatile int8_t                     state;
    int8_t                              reserved;
};
typedef struct dispatch_item* dispatch_item_t;


// A convenience macro to initialize a dispatch item before it is submitted to
// a dispatcher. Note that you still need to set up 'itemFunc' and possibly
// 'flags' before you submit the item.
#define DISPATCH_ITEM_INIT  (struct dispatch_item){0}


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
#define DISPATCH_PRI_HIGHEST    5
#define DISPATCH_PRI_NORMAL     0
#define DISPATCH_PRI_LOWEST     -6

#define DISPATCH_PRI_COUNT     12


// Optional dispatch callbacks. A dispatcher implements a FIFO work queue by
// default. You may override the item insertion and retrieval functions to
// implement a custom queuing and dequeuing algorithm. The 'insert_item'
// function receives a work queue and a dispatch item and it is responsible for
// inserting the item in the work queue. The 'remove_item' function receives a
// work queue and it is responsible for selecting and removing the next dispatch
// item that the dispatcher worker should execute.
typedef struct dispatch_callbacks {
    int                         version;        // Version 0
    int                         (*insert_item)(dispatch_item_t _Nonnull item, SList* _Nonnull queue);
    dispatch_item_t _Nullable   (*remove_item)(SList* _Nonnull queue);
} dispatch_callbacks_t;


typedef struct dispatch_attr {
    size_t  minConcurrency;
    size_t  maxConcurrency;
    int     qos;
    int     priority;
    dispatch_callbacks_t* _Nullable cb;
} dispatch_attr_t;


// Initializes a dispatch attribute object to set up a serial queue with
// interactive priority.
#define DISPATCH_ATTR_INIT_SERIAL_INTERACTIVE       (dispatch_attr_t){1, 1, DISPATCH_QOS_INTERACTIVE, DISPATCH_PRI_NORMAL, NULL}

// Initailizes a dispatch attribute object to set up a concurrent queue with
// '__n' virtual processors and utility priority.
#define DISPATCH_ATTR_INIT_CONCURRENT_UTILITY(__n)  (dispatch_attr_t){1, __n, DISPATCH_QOS_UTILITY, DISPATCH_PRI_NORMAL, NULL}


// Creates a new dispatcher based on the provided dispatcher attributes.
extern dispatch_t _Nullable dispatch_create(const dispatch_attr_t* _Nonnull attr);

// Destroys the given dispatcher. Returns -1 and sets errno to EBUSY if the
// dispatcher wasn't terminated, is still in the process of terminating or there
// are still unjoined joinable dispatch items outstanding.
extern int dispatch_destroy(dispatch_t _Nullable self);


// Submits the dispatch item 'item' to the dispatcher 'self'. The item will be
// asynchronously executed as soon as possible. Note that the dispatcher takes
// ownership of 'item' until the item is done processing. Once an item is done
// processing the dispatcher either calls the 'retireFunc' of the item, if the
// item is not joinable, or it enqueues the item on a result queue if it is
// joinable. A joinable item must be joined by calling dispatch_join() on it.
extern int dispatch_submit(dispatch_t _Nonnull self, dispatch_item_t _Nonnull item);

// Waits for the execution of 'item' to finish and removes the item from the
// result queue. Note that this function does not call the 'retireFunc' of the
// item. You are expected to retire the item after dispatch_join() returns and
// you no longer need access to 'item'. This function effectively transfers
// ownership of 'item' back to you.
extern int dispatch_join(dispatch_t _Nonnull self, dispatch_item_t _Nonnull item);


// Convenience function which creates a simple item to execute a function with
// a single argument asynchronously. The item is created, managed and retired by
// the dispatcher itself. The asynchronous function can not return a result. Use
// dispatch_sync() instead if you need a result back from the function.   
typedef void (*dispatch_async_func_t)(void* _Nullable context);
extern int dispatch_async(dispatch_t _Nonnull self, dispatch_async_func_t _Nonnull func, void* _Nullable context);


// A convenience function to synchronously execute a function on the dispatcher
// queue. The function may return an int size value. This value is returned as
// the dispatch_sync() result.
typedef int (*dispatch_sync_func_t)(void* _Nullable context);
extern int dispatch_sync(dispatch_t _Nonnull self, dispatch_sync_func_t _Nonnull func, void* _Nullable context);


// Initiates the termination of a dispatcher. Note that termination is an
// inherently asynchronous operation that may take a while. This function will
// never cancel an item that is currently in processing state. However it may
// cancel items that are still in pending state. Pass true for 'cancel' if all
// still pending items should be canceled and not executed. Pass false for
// 'cancel' if all pending items should be allowed to execute before the
// dispatcher completes termination. Note that the dispatcher will no longer
// accept new items as soon as this function returns. Any attempt to submit a
// new item to the dispatcher will be rejected with an ETERMINATED error.
extern void dispatch_terminate(dispatch_t _Nonnull self, bool cancel);

// Blocks the caller until the dispatcher has completed termination. It is safe
// to call dispatch_destroy() on the dispatcher instance once this function has
// returned.
extern int dispatch_await_termination(dispatch_t _Nonnull self);

__CPP_END

#endif /* _DISPATCH_H */
