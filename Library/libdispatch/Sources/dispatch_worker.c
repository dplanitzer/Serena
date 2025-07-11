//
//  dispatch_worker.c
//  libdispatch
//
//  Created by Dietmar Planitzer on 7/10/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "dispatch_priv.h"
#include <stdlib.h>

#define VP_PRIORITIES_RESERVED_LOW  2

static void _dispatch_worker_run(dispatch_worker_t _Nonnull self);


dispatch_worker_t _Nullable _dispatch_worker_create(const dispatch_attr_t* _Nonnull attr, dispatch_t _Nonnull owner)
{
    dispatch_worker_t self = malloc(sizeof(struct dispatch_worker));

    if (self == NULL) {
        return NULL;
    }

    self->worker_qe = LISTNODE_INIT;
    self->work_queue = SLIST_INIT;
    self->work_count = 0;

    self->cb = attr->cb;
    self->owner = owner;

    sigemptyset(&self->hotsigs);
    sigaddset(&self->hotsigs, SIGDISPATCH);

    vcpu_acquire_params_t params;
    params.func = (vcpu_start_t)_dispatch_worker_run;
    params.arg = self;
    params.stack_size = 0;
    params.groupid = new_vcpu_groupid();
    params.priority = attr->qos * DISPATCH_PRI_COUNT + (attr->priority + DISPATCH_PRI_COUNT / 2) + VP_PRIORITIES_RESERVED_LOW;
    params.flags = 0;

    self->vcpu = vcpu_acquire(&params);
    if (self->vcpu == NULL) {
        free(self);
        return NULL;
    }
    self->id = vcpu_id(self->vcpu);

    vcpu_resume(self->vcpu);

    return self;
}

void _dispatch_worker_destroy(dispatch_worker_t _Nullable self)
{
    if (self) {
        self->owner = NULL;
        self->cb = NULL;
        // vcpu is relinquished via _dispatch_relinquish_worker()
        free(self);
    }
}

void _dispatch_worker_submit(dispatch_worker_t _Nonnull _Locked self, dispatch_item_t _Nonnull item)
{
    if (self->cb->insert_item == NULL) {
        SList_InsertAfterLast(&self->work_queue, &item->qe);
    }
    else {
        self->cb->insert_item(item, &self->work_queue);
    }
    self->work_count++;
    item->state = DISPATCH_STATE_PENDING;

    sigsend(SIG_SCOPE_VCPU, self->id, SIGDISPATCH);
}

static void _dispatch_worker_retire_item(dispatch_worker_t _Nonnull _Locked self, dispatch_item_t _Nonnull item)
{
    if ((item->flags & DISPATCH_ITEM_JOINABLE) != 0) {
        _dispatch_zombify_item(self->owner, item);
    }
    else if (item->version < _DISPATCH_ITEM_TYPE_BASE) {
        _dispatch_cache_item(self->owner, (dispatch_cachable_item_t)item);
    }
    else if (item->retireFunc) {
        item->retireFunc(item);
    }
}

// Cancels all items that are still on the worker's work queue
void _dispatch_worker_drain(dispatch_worker_t _Nonnull _Locked self)
{
    SList_ForEach(&self->work_queue, SListNode, {
        dispatch_item_t cip = (dispatch_item_t)pCurNode;

        _dispatch_worker_retire_item(self, cip);
    });

    self->work_queue = SLIST_INIT;
    self->work_count = 0;
}


static dispatch_item_t _Nullable _get_next_item(dispatch_worker_t _Nonnull self, mutex_t* _Nonnull mp)
{
    volatile int* statep = &self->owner->state;
    dispatch_item_t item;
    siginfo_t si;

    while (*statep == _DISPATCHER_STATE_ACTIVE) {
        if (self->cb->remove_item == NULL) {
            item = (dispatch_item_t) SList_RemoveFirst(&self->work_queue);
        }
        else {
            item = self->cb->remove_item(&self->work_queue);
        }

        if (item) {
            self->work_count--;
            return item;
        }


        mutex_unlock(mp);
        sigwait(&self->hotsigs, &si);
        mutex_lock(mp);
    }

    return NULL;
}

static void _dispatch_worker_run(dispatch_worker_t _Nonnull self)
{
    mutex_t* mp = &(self->owner->mutex);

    mutex_lock(mp);

    for (;;) {
        dispatch_item_t item = _get_next_item(self, mp);

        if (item == NULL) {
            break;
        }

        item->state = DISPATCH_STATE_EXECUTING;
        mutex_unlock(mp);


        item->itemFunc(item);


        mutex_lock(mp);
        item->state = DISPATCH_STATE_DONE;

        _dispatch_worker_retire_item(self, item);
    }

    // Takes care of unlocking 'mp'
    _dispatch_relinquish_worker(self->owner, self);
}
