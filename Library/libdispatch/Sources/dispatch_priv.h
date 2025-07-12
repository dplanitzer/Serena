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
#include <stdnoreturn.h>
#include <sys/condvar.h>
#include <sys/vcpu.h>
#include <stdio.h>

__CPP_BEGIN

struct dispatch_worker {
    ListNode                                worker_qe;

    SList                                   work_queue;
    size_t                                  work_count;

    vcpu_t _Nonnull                         vcpu;
    vcpuid_t                                id;

    sigset_t                                hotsigs;

    const dispatch_callbacks_t* _Nonnull    cb;
    dispatch_t _Nonnull                     owner;
};
typedef struct dispatch_worker* dispatch_worker_t;

extern dispatch_worker_t _Nullable _dispatch_worker_create(const dispatch_attr_t* _Nonnull attr, dispatch_t _Nonnull owner);
extern void _dispatch_worker_destroy(dispatch_worker_t _Nullable self);

extern void _dispatch_worker_submit(dispatch_worker_t _Nonnull _Locked self, dispatch_item_t _Nonnull item);
extern void _dispatch_worker_drain(dispatch_worker_t _Nonnull _Locked self);


// Internal item flags
#define _DISPATCH_ITEM_PUBLIC_MASK  0x00ff
#define _DISPATCH_ITEM_CACHEABLE    0x100


// Dispatcher state
#define _DISPATCHER_STATE_ACTIVE        0
#define _DISPATCHER_STATE_TERMINATING   1
#define _DISPATCHER_STATE_TERMINATED    2


struct dispatch {
    mutex_t                 mutex;
    cond_t                  cond;
    dispatch_attr_t         attr;
    dispatch_callbacks_t    cb;

    List                    workers;
    size_t                  worker_count;

    SList                   zombies;        // Items that are done and joinable

    SList                   item_cache;
    size_t                  item_cache_count;

    volatile int            state;
};


#define DISPATCH_MAX_CACHE_COUNT    8

struct dispatch_cachable_item {
    struct dispatch_item    super;
    size_t                  size;
};
typedef struct dispatch_cachable_item* dispatch_cacheable_item_t;


struct dispatch_async_item {
    struct dispatch_cachable_item   super;
    dispatch_async_func_t _Nonnull  func;
    void* _Nullable                 context;
};
typedef struct dispatch_async_item* dispatch_async_item_t;

struct dispatch_sync_item {
    struct dispatch_cachable_item   super;
    dispatch_sync_func_t _Nonnull   func;
    void* _Nullable                 context;
    int                             result;
};
typedef struct dispatch_sync_item* dispatch_sync_item_t;


extern void _dispatch_zombify_item(dispatch_t _Nonnull _Locked self, dispatch_item_t _Nonnull item);
extern void _dispatch_cache_item(dispatch_t _Nonnull _Locked self, dispatch_cacheable_item_t _Nonnull item);

extern _Noreturn _dispatch_relinquish_worker(dispatch_t _Nonnull _Locked self, dispatch_worker_t _Nonnull worker);

__CPP_END

#endif /* _DISPATCH_PRIV_H */
