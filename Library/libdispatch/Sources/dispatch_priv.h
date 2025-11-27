//
//  dispatch_priv.h
//  libdispatch
//
//  Created by Dietmar Planitzer on 7/10/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _DISPATCH_PRIV_H
#define _DISPATCH_PRIV_H 1

#include <dispatch.h>
#include <signal.h>
#include <sys/cnd.h>
#include <sys/timespec.h>
#include <sys/vcpu.h>

__CPP_BEGIN

#define _DISPATCH_MAX_ITEM_CACHE_COUNT  8

struct dispatch_cacheable_item {
    struct dispatch_item    super;
    size_t                  size;
};
typedef struct dispatch_cacheable_item* dispatch_cacheable_item_t;


struct dispatch_async_item {
    struct dispatch_cacheable_item  super;
    dispatch_async_func_t _Nonnull  func;
    void* _Nullable                 arg;
};
typedef struct dispatch_async_item* dispatch_async_item_t;

struct dispatch_sync_item {
    struct dispatch_cacheable_item  super;
    dispatch_sync_func_t _Nonnull   func;
    void* _Nullable                 arg;
    int                             result;
};
typedef struct dispatch_sync_item* dispatch_sync_item_t;


#define _DISPATCH_MAX_TIMER_CACHE_COUNT 4

// A particular timer instance may appear at most once on the timer queue.
struct dispatch_timer {
    SListNode                   timer_qe;
    struct timespec             deadline;   // Time when the timer fires next
    struct timespec             interval;   // Time interval until next time the timer should fire (if repeating) 
    dispatch_item_t _Nonnull    item;
};
typedef struct dispatch_timer* dispatch_timer_t;


typedef struct dispatch_work {
    dispatch_timer_t _Nullable  timer;  // timer of 'item' if it is timed; NULL otherwise
    dispatch_item_t _Nullable   item;   // points to the currently executing item; NULL if worker isn't executing anything
} dispatch_work_t;


typedef struct dispatch_sigmon {
    SList   handlers;
    int     handlers_count;
} dispatch_sigmon_t;


// _dispatch_worker_create() adoption mode
#define _DISPATCH_ACQUIRE_VCPU      0
#define _DISPATCH_ADOPT_CALLER_VCPU 1
#define _DISPATCH_ADOPT_MAIN_VCPU   2

struct dispatch_worker {
    ListNode                worker_qe;

    SList                   work_queue;
    size_t                  work_count;

    dispatch_work_t         current;    // Currently executing work

    vcpu_t _Nonnull         vcpu;
    vcpuid_t                id;

    sigset_t                hotsigs;

    dispatch_t _Nonnull     owner;

    int8_t                  adoption;  // _DISPATCH_XXX_VCPU; tells us whether the worker acquired or adopted its vcpu
    bool                    allow_relinquish;   // whether the worker is free to relinquish or not
    bool                    is_suspended;   // set to true by the worker when it has picked up on the dispatcher suspending state
};
typedef struct dispatch_worker* dispatch_worker_t;

extern dispatch_worker_t _Nullable _dispatch_worker_create(dispatch_t _Nonnull owner, int ownership);
extern void _dispatch_worker_destroy(dispatch_worker_t _Nullable self);

extern void _dispatch_worker_wakeup(dispatch_worker_t _Nonnull _Locked self);
extern void _dispatch_worker_submit(dispatch_worker_t _Nonnull _Locked self, dispatch_item_t _Nonnull item, bool doWakeup);
extern dispatch_item_t _Nullable _dispatch_worker_find_item(dispatch_worker_t _Nonnull self, dispatch_item_func_t _Nonnull func);
extern bool _dispatch_worker_cancel_item(dispatch_worker_t _Nonnull self, int flags, dispatch_item_t _Nonnull item);
extern void _dispatch_worker_drain(dispatch_worker_t _Nonnull _Locked self);

extern void _dispatch_worker_run(dispatch_worker_t _Nonnull self);

extern vcpu_key_t __os_dispatch_key;
#define _dispatch_worker_current() \
(dispatch_worker_t)vcpu_specific(__os_dispatch_key)


// Internal item flags
#define _DISPATCH_ITEM_FLAG_AWAITABLE   DISPATCH_SUBMIT_AWAITABLE
#define _DISPATCH_ITEM_FLAG_ABSTIME     DISPATCH_SUBMIT_ABSTIME

// Item is owned by the dispatcher and should be moved back to the work item
// cache when done.
#define _DISPATCH_ITEM_FLAG_CACHEABLE   0x40

// The item is repeating (eg associated with a repeating timer) and should be
// auto-resubmitted if not cancelled. 
#define _DISPATCH_ITEM_FLAG_REPEATING   0x80


// Item type
#define _DISPATCH_TYPE_WORK_ITEM        0x01
#define _DISPATCH_TYPE_TIMED_ITEM       0x02
#define _DISPATCH_TYPE_SIGNAL_ITEM      0x03


// Dispatcher state
#define _DISPATCHER_STATE_ACTIVE        0
#define _DISPATCHER_STATE_SUSPENDING    1
#define _DISPATCHER_STATE_SUSPENDED     2
#define _DISPATCHER_STATE_TERMINATING   3
#define _DISPATCHER_STATE_TERMINATED    4


struct dispatch {
    mtx_t               mutex;
    cnd_t               cond;
    dispatch_attr_t     attr;
    vcpuid_t            groupid;        // Constant over lifetime

    List                workers;        // Each worker has its own work item queue
    size_t              worker_count;

    SList               zombie_items;   // Items that are done and joinable

    SList               item_cache;
    size_t              item_cache_count;

    SList               timers;         // The timer queue is shared by all workers
    SList               timer_cache;
    size_t              timer_cache_count;

    dispatch_sigmon_t* _Nullable    sigmons;
    sigset_t            alloced_sigs;

    volatile int        state;
    int                 suspension_count;

    char                name[DISPATCH_MAX_NAME_LENGTH + 1];
};

extern int _dispatch_rearm_timer(dispatch_t _Nonnull _Locked self, dispatch_timer_t _Nonnull timer);
extern void _dispatch_retire_item(dispatch_t _Nonnull _Locked self, dispatch_item_t _Nonnull item);
extern void _dispatch_retire_timer(dispatch_t _Nonnull _Locked self, dispatch_timer_t _Nonnull timer);
extern void _dispatch_zombify_item(dispatch_t _Nonnull _Locked self, dispatch_item_t _Nonnull item);
extern void _dispatch_cache_item(dispatch_t _Nonnull _Locked self, dispatch_cacheable_item_t _Nonnull item);
extern void _dispatch_wakeup_all_workers(dispatch_t _Nonnull self);
extern dispatch_cacheable_item_t _Nullable _dispatch_acquire_cached_item(dispatch_t _Nonnull _Locked self, size_t nbytes, dispatch_item_func_t func);
static int _dispatch_acquire_worker_with_ownership(dispatch_t _Nonnull _Locked self, int ownership);
extern int _dispatch_acquire_worker(dispatch_t _Nonnull _Locked self);
extern bool _dispatch_isactive(dispatch_t _Nonnull _Locked self);


extern dispatch_timer_t _Nullable _dispatch_find_timer(dispatch_t _Nonnull self, dispatch_item_func_t _Nonnull func);
extern void _dispatch_cancel_timer(dispatch_t _Nonnull self, int flags, dispatch_item_t _Nonnull item);
extern void _dispatch_cache_timer(dispatch_t _Nonnull _Locked self, dispatch_timer_t _Nonnull timer);
extern void _dispatch_drain_timers(dispatch_t _Nonnull _Locked self);

extern void _dispatch_cancel_signal_item(dispatch_t _Nonnull self, int flags, dispatch_item_t _Nonnull item);
extern void _dispatch_submit_items_for_signal(dispatch_t _Nonnull _Locked self, int signo, dispatch_worker_t _Nonnull worker);
extern void _dispatch_rearm_signal_item(dispatch_t _Nonnull _Locked self, dispatch_item_t _Nonnull item);

extern _Noreturn _dispatch_relinquish_worker(dispatch_t _Nonnull _Locked self, dispatch_worker_t _Nonnull worker);

extern void _async_adapter_func(dispatch_item_t _Nonnull item);

__CPP_END

#endif /* _DISPATCH_PRIV_H */
