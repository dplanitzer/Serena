//
//  kdispatch_priv.h
//  kernel
//
//  Created by Dietmar Planitzer on 7/10/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _KDISPATCH_PRIV_H
#define _KDISPATCH_PRIV_H 1

#include "kdispatch.h"
#include <kern/assert.h>
#include <kern/kalloc.h>
#include <kern/kernlib.h>
#include <kern/limits.h>
#include <kern/signal.h>
#include <kern/timespec.h>
#include <kern/types.h>
#include <sched/cnd.h>
#include <sched/mtx.h>
#include <sched/vcpu.h>
#include <sched/waitqueue.h>

__CPP_BEGIN

// Permissible item state transitions:
// IDLE         -> SCHEDULED
// SCHEDULED    -> EXECUTING | CANCELLED
// EXECUTING    -> FINISHED | CANCELLED
// FINISHED     -> SCHEDULED
// CANCELLED    -> SCHEDULED
//
// The transition from SCHEDULED | EXECUTING to CANCELLED is done indirectly by
// first setting the _KDISPATCH_ITEM_FLAG_CANCELLED flag on the item to indicate
// that the item should be cancelled. Since cancelling is a voluntary and
// cooperative task, the item (if it is already in EXECUTING state) has to
// recognize the cancel request and act on it before we can transition the item
// to CANCELLED state.

#define _KDISPATCH_MAX_CONV_ITEM_CACHE_COUNT 8
#define _KDISPATCH_MAX_TIMER_CACHE_COUNT     4


struct kdispatch_conv_item {
    struct kdispatch_item   super;
    int _Nonnull            (*func)(void*);
    void* _Nullable         arg;
    int                     result;
};
typedef struct kdispatch_conv_item* kdispatch_conv_item_t;


struct kdispatch_timer {
    SListNode                   timer_qe;
    kdispatch_item_t _Nonnull   item;
    struct timespec             deadline;   // Time when the timer fires next
    struct timespec             interval;   // Time interval until next time the timer should fire (if repeating) 
};
typedef struct kdispatch_timer* kdispatch_timer_t;


struct kdispatch_sigtrap {
    SList   monitors;
    int     count;
};
typedef struct kdispatch_sigtrap* kdispatch_sigtrap_t;


struct kdispatch_worker {
    ListNode                    worker_qe;

    SList                       work_queue;
    size_t                      work_count;

    kdispatch_item_t _Nullable  current_item;   // currently executing item
    kdispatch_timer_t _Nullable current_timer;  // and timer

    vcpu_t _Nonnull             vcpu;

    sigset_t                    hotsigs;
    struct waitqueue            wq;

    kdispatch_t _Nonnull        owner;

    int8_t                      adoption;  // _KDISPATCH_XXX_VCPU; tells us whether the worker acquired or adopted its vcpu
    bool                        allow_relinquish;   // whether the worker is free to relinquish or not
    bool                        is_suspended;   // set to true by the worker when it has picked up on the dispatcher suspending state
};
typedef struct kdispatch_worker* kdispatch_worker_t;

extern errno_t _kdispatch_worker_create(kdispatch_t _Nonnull owner, kdispatch_worker_t _Nullable * _Nonnull pOutSelf);
extern void _kdispatch_worker_destroy(kdispatch_worker_t _Nullable self);

extern void _kdispatch_worker_wakeup(kdispatch_worker_t _Nonnull _Locked self);
extern void _kdispatch_worker_submit(kdispatch_worker_t _Nonnull _Locked self, kdispatch_item_t _Nonnull item, bool doWakeup);
extern kdispatch_item_t _Nullable _kdispatch_worker_find_item(kdispatch_worker_t _Nonnull self, kdispatch_item_func_t _Nonnull func, void* _Nullable arg);
extern bool _kdispatch_worker_withdraw_item(kdispatch_worker_t _Nonnull self, kdispatch_item_t _Nonnull item);
extern void _kdispatch_worker_drain(kdispatch_worker_t _Nonnull _Locked self);

extern void _kdispatch_worker_run(kdispatch_worker_t _Nonnull self);

#define _kdispatch_worker_current() \
(kdispatch_worker_t)vcpu_current()->udata


// Internal item flags
#define _KDISPATCH_ITEM_FLAG_AWAITABLE   KDISPATCH_SUBMIT_AWAITABLE

// Item is cancelled and should enter cancelled state once execution has finished.
#define _KDISPATCH_ITEM_FLAG_CANCELLED   0x20

// Item is owned by the dispatcher and should be moved back to the work item
// cache when done.
#define _KDISPATCH_ITEM_FLAG_CACHEABLE   0x40

// The item is repeating (eg associated with a repeating timer) and should be
// auto-resubmitted if not cancelled. 
#define _KDISPATCH_ITEM_FLAG_REPEATING   0x80


// Item type
#define _KDISPATCH_TYPE_USER_ITEM        0x01    // user owned
#define _KDISPATCH_TYPE_USER_SIGNAL_ITEM 0x02    // user owned
#define _KDISPATCH_TYPE_USER_TIMER       0x03    // user owned
#define _KDISPATCH_TYPE_CONV_ITEM        0x04    // cacheable, dispatcher owned
#define _KDISPATCH_TYPE_CONV_TIMER       0x05    // cacheable, dispatcher owned


// Dispatcher state
#define _DISPATCHER_STATE_ACTIVE        0
#define _DISPATCHER_STATE_SUSPENDING    1
#define _DISPATCHER_STATE_SUSPENDED     2
#define _DISPATCHER_STATE_TERMINATING   3
#define _DISPATCHER_STATE_TERMINATED    4


// _kdispatch_ensure_worker_capacity() call reason
#define _KDISPATCH_EWC_WORK_ITEM    0
#define _KDISPATCH_EWC_SIGNAL_ITEM  1
#define _KDISPATCH_EWC_TIMER        2


struct dispatch {
    mtx_t                           mutex;
    cnd_t                           cond;
    kdispatch_attr_t                attr;
    vcpuid_t                        groupid;        // Constant over lifetime

    List                            workers;        // Each worker has its own work item queue
    size_t                          worker_count;

    SList                           zombie_items;   // Items that are done and joinable

    SList                           item_cache;
    size_t                          item_cache_count;

    SList                           timers;         // The timer queue is shared by all workers
    SList                           timer_cache;
    size_t                          timer_cache_count;

    kdispatch_sigtrap_t _Nullable   sigtraps;
    sigset_t                        alloced_sigs;

    volatile int                    state;
    int                             suspension_count;

    char                            name[KDISPATCH_MAX_NAME_LENGTH + 1];
};

extern void _kdispatch_retire_item(kdispatch_t _Nonnull _Locked self, kdispatch_item_t _Nonnull item);
extern void _kdispatch_zombify_item(kdispatch_t _Nonnull _Locked self, kdispatch_item_t _Nonnull item);
extern void _kdispatch_cache_item(kdispatch_t _Nonnull _Locked self, kdispatch_item_t _Nonnull item);
extern kdispatch_item_t _Nullable _kdispatch_acquire_cached_conv_item(kdispatch_t _Nonnull _Locked self, kdispatch_item_func_t func);
extern void _kdispatch_wakeup_all_workers(kdispatch_t _Nonnull self);
extern int _kdispatch_ensure_worker_capacity(kdispatch_t _Nonnull self, int reason);
extern errno_t _kdispatch_acquire_worker(kdispatch_t _Nonnull _Locked self);
extern bool _kdispatch_isactive(kdispatch_t _Nonnull _Locked self);


extern void _kdispatch_rearm_timer(kdispatch_t _Nonnull _Locked self, kdispatch_timer_t _Nonnull timer);
extern kdispatch_timer_t _Nullable _kdispatch_find_timer(kdispatch_t _Nonnull self, kdispatch_item_func_t _Nonnull func, void* _Nullable arg);
extern void _kdispatch_withdraw_timer_for_item(kdispatch_t _Nonnull self, kdispatch_item_t _Nonnull item);
extern void _kdispatch_drain_timers(kdispatch_t _Nonnull _Locked self);
extern void _kdispatch_retire_timer(kdispatch_t _Nonnull _Locked self, kdispatch_timer_t _Nonnull timer);

extern void _kdispatch_withdraw_signal_item(kdispatch_t _Nonnull self, kdispatch_item_t _Nonnull item);
extern void _kdispatch_retire_signal_item(kdispatch_t _Nonnull self, kdispatch_item_t _Nonnull item);
extern void _kdispatch_submit_items_for_signal(kdispatch_t _Nonnull _Locked self, int signo, kdispatch_worker_t _Nonnull worker);
extern void _kdispatch_rearm_signal_item(kdispatch_t _Nonnull _Locked self, kdispatch_item_t _Nonnull item);

extern _Noreturn _kdispatch_relinquish_worker(kdispatch_t _Nonnull _Locked self, kdispatch_worker_t _Nonnull worker);

extern void _async_adapter_func(kdispatch_item_t _Nonnull item);
extern bool _kdispatch_item_has_func(kdispatch_item_t _Nonnull item, kdispatch_item_func_t _Nonnull func, void* _Nullable arg);

__CPP_END

#endif /* _KDISPATCH_PRIV_H */
