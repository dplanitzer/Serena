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


typedef void (*dispatch_item_func_t)(struct dispatch_item* _Nonnull item);
typedef void (*dispatch_retire_func_t)(struct dispatch_item* _Nonnull item);

#define DISPATCH_ITEM_JOINABLE  1

#define DISPATCH_STATE_IDLE         0
#define DISPATCH_STATE_PENDING      1
#define DISPATCH_STATE_EXECUTING    2
#define DISPATCH_STATE_DONE         3
#define DISPATCH_STATE_CANCELLED    4


struct dispatch_item {
    SListNode                           qe;
    dispatch_item_func_t _Nonnull       itemFunc;
    dispatch_retire_func_t _Nullable    retireFunc;
    int8_t                              version;
    volatile int8_t                     state;
    uint8_t                             flags;
    int8_t                              reserved;
};
typedef struct dispatch_item* dispatch_item_t;

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

#define DISPATCH_ATTR_INIT_SERIAL_INTERACTIVE       (dispatch_attr_t){1, 1, DISPATCH_QOS_INTERACTIVE, DISPATCH_PRI_NORMAL, NULL}
#define DISPATCH_ATTR_INIT_CONCURRENT_UTILITY(__n)  (dispatch_attr_t){1, __n, DISPATCH_QOS_UTILITY, DISPATCH_PRI_NORMAL, NULL}


extern dispatch_t _Nullable dispatch_create(const dispatch_attr_t* _Nonnull attr);
extern int dispatch_destroy(dispatch_t _Nullable self);

extern int dispatch_submit(dispatch_t _Nonnull self, dispatch_item_t _Nonnull item);
extern int dispatch_join(dispatch_t _Nonnull self, dispatch_item_t _Nonnull item);


typedef void (*dispatch_async_func_t)(void* _Nullable context);
extern int dispatch_async(dispatch_t _Nonnull self, dispatch_async_func_t _Nonnull func, void* _Nullable context);


typedef int (*dispatch_sync_func_t)(void* _Nullable context);
extern int dispatch_sync(dispatch_t _Nonnull self, dispatch_sync_func_t _Nonnull func, void* _Nullable context);


extern void dispatch_terminate(dispatch_t _Nonnull self, bool cancel);
extern int dispatch_await_termination(dispatch_t _Nonnull self);

__CPP_END

#endif /* _DISPATCH_H */
