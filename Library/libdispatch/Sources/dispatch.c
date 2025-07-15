//
//  dispatch.c
//  libdispatch
//
//  Created by Dietmar Planitzer on 7/10/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "dispatch_priv.h"
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>


static struct dispatch  g_main_dispatcher;
dispatch_t DISPATCH_MAIN = &g_main_dispatcher;

static bool _dispatch_init(dispatch_t _Nonnull self, const dispatch_attr_t* _Nonnull attr, int ownership)
{
    if (attr->maxConcurrency < 1 || attr->maxConcurrency > INT8_MAX || attr->minConcurrency > attr->maxConcurrency) {
        errno = EINVAL;
        return false;
    }
    if (attr->qos < 0 || attr->qos >= DISPATCH_QOS_COUNT) {
        errno = EINVAL;
        return false;
    }
    if (attr->priority < DISPATCH_PRI_LOWEST || attr->priority >= DISPATCH_PRI_HIGHEST) {
        errno = EINVAL;
        return false;
    }


    if (mutex_init(&self->mutex) != 0) {
        return false;
    }

    self->attr = *attr;
    self->attr.cb = &self->cb;
    if (attr->cb) {
        self->cb = *(attr->cb);
    }

    self->groupid = (ownership == _DISPATCH_ACQUIRE_VCPU) ? new_vcpu_groupid() : vcpu_groupid(vcpu_self());
    self->workers = LIST_INIT;
    self->worker_count = 0;
    self->item_cache = SLIST_INIT;
    self->item_cache_count = 0;
    self->zombies = SLIST_INIT;
    self->timers = SLIST_INIT;
    self->timer_cache = SLIST_INIT;
    self->timer_cache_count = 0;
    self->state = _DISPATCHER_STATE_ACTIVE;
    self->signature = _DISPATCH_SIGNATURE;

    if (cond_init(&self->cond) != 0) {
        return false;
    }

    for (size_t i = 0; i < attr->minConcurrency; i++) {
        if (_dispatch_acquire_worker_with_ownership(self, ownership) != 0) {
            return false;
        }
    }

    if (attr->name && attr->name[0] != '\0') {
        strncpy(self->name, attr->name, DISPATCH_MAX_NAME_LENGTH);
    }

    return true;
}

dispatch_t _Nullable dispatch_create(const dispatch_attr_t* _Nonnull attr)
{
    dispatch_t self = calloc(1, sizeof(struct dispatch));
    
    if (self) {
        if (_dispatch_init(self, attr, _DISPATCH_ACQUIRE_VCPU)) {
            return self;
        }

        if (self->state == _DISPATCHER_STATE_ACTIVE) {
            dispatch_destroy(self);
        }
    }
    return NULL;
}

int dispatch_destroy(dispatch_t _Nullable self)
{
    if (self) {
        if (self->state < _DISPATCHER_STATE_TERMINATED || !SList_IsEmpty(&self->zombies)) {
            errno = EBUSY;
            return -1;
        }


        SList_ForEach(&self->timer_cache, ListNode, {
            dispatch_timer_t ctp = (dispatch_timer_t)pCurNode;

            free(ctp);
        });
        self->timer_cache = SLIST_INIT;

        SList_ForEach(&self->item_cache, ListNode, {
            dispatch_cacheable_item_t cip = (dispatch_cacheable_item_t)pCurNode;

            free(cip);
        });
        self->item_cache = SLIST_INIT;

        self->workers = LIST_INIT;
        self->zombies = SLIST_INIT;
        self->timers = SLIST_INIT;

        cond_deinit(&self->cond);
        mutex_deinit(&self->mutex);

        free(self);
    }

    return 0;
}


////////////////////////////////////////////////////////////////////////////////
// MARK: Workers

static int _dispatch_acquire_worker_with_ownership(dispatch_t _Nonnull _Locked self, int ownership)
{
    dispatch_worker_t worker = _dispatch_worker_create(self, ownership);

    if (worker) {
        List_InsertAfterLast(&self->workers, &worker->worker_qe);
        self->worker_count++;

        return 0;
    }

    return -1;
}

int _dispatch_acquire_worker(dispatch_t _Nonnull _Locked self)
{
    return _dispatch_acquire_worker_with_ownership(self, _DISPATCH_ACQUIRE_VCPU);
}

_Noreturn _dispatch_relinquish_worker(dispatch_t _Nonnull _Locked self, dispatch_worker_t _Nonnull worker)
{
    const int ownership = worker->ownership;

    List_Remove(&self->workers, &worker->worker_qe);
    self->worker_count--;

    _dispatch_worker_destroy(worker);

    cond_broadcast(&self->cond);
    mutex_unlock(&self->mutex);

    if (ownership == _DISPATCH_ACQUIRE_VCPU) {
        vcpu_relinquish_self();
    }
}

void _dispatch_wakeup_all_workers(dispatch_t _Nonnull self)
{
    List_ForEach(&self->workers, ListNode, {
        dispatch_worker_t cwp = (dispatch_worker_t)pCurNode;

        _dispatch_worker_wakeup(cwp);
    });
}


////////////////////////////////////////////////////////////////////////////////
// MARK: Work Items

static int _dispatch_submit(dispatch_t _Nonnull _Locked self, dispatch_item_t _Nonnull item)
{
    dispatch_worker_t best_wp = NULL;
    size_t best_wc = SIZE_MAX;

    if (item->state != DISPATCH_STATE_IDLE) {
        errno = EBUSY;
        return -1;
    }


    // Find the worker with the least amount of work scheduled
    if (self->worker_count > 1) {
        List_ForEach(&self->workers, ListNode, {
            dispatch_worker_t cwp = queue_entry_as(pCurNode, worker_qe, dispatch_worker);

            if (cwp->work_count < best_wc) {
                best_wc = cwp->work_count;
                best_wp = cwp;
            }
        });
    }
    else {
        best_wp = (dispatch_worker_t)self->workers.first;
        best_wc = best_wp->work_count;
    }


    // Worker:
    // - need at least one
    // - spawn another one if the 'best' worker has too much stuff queued and
    //   we haven't reached the max number of workers yet
    if (self->worker_count == 0 || (best_wc > 4 && self->worker_count < self->attr.maxConcurrency)) {
        const int r = _dispatch_acquire_worker(self);

        // Don't make the submit fail outright if we can't allocate a new worker
        // and this was just about adding one more.
        if (r != 0 && self->worker_count == 0) {
            return -1;
        }
    }

    item->state = DISPATCH_STATE_PENDING;
    item->qe = SLISTNODE_INIT;


    // Enqueue the work item at the worker that we found and notify it
    _dispatch_worker_submit(best_wp, item);

    return 0;
}

void _dispatch_retire_item(dispatch_t _Nonnull _Locked self, dispatch_item_t _Nonnull item)
{
    if ((item->flags & DISPATCH_ITEM_AWAITABLE) != 0) {
        _dispatch_zombify_item(self, item);
    }
    else if ((item->flags & _DISPATCH_ITEM_CACHEABLE) != 0) {
        _dispatch_cache_item(self, (dispatch_cacheable_item_t)item);
    }
    else if (item->retireFunc) {
        item->retireFunc(item);
    }
    else {
        free(item);
    }
}

static int _dispatch_await(dispatch_t _Nonnull _Locked self, dispatch_item_t _Nonnull item)
{
    int r = 0;

    while (item->state < DISPATCH_STATE_DONE) {
        r = cond_wait(&self->cond, &self->mutex);
        if (r != 0) {
            break;
        }
    }

    dispatch_item_t pip = NULL;
    SList_ForEach(&self->zombies, ListNode, {
        dispatch_item_t cip = (dispatch_item_t)pCurNode;

        if (cip == item) {
            break;
        }
        pip = cip;
    });
    SList_Remove(&self->zombies, &pip->qe, &item->qe);

    return r;
}

void _dispatch_zombify_item(dispatch_t _Nonnull _Locked self, dispatch_item_t _Nonnull item)
{
    SList_InsertAfterLast(&self->zombies, &item->qe);
    cond_broadcast(&self->cond);
}

static dispatch_item_t _Nullable _dispatch_find_item(dispatch_t _Nonnull self, dispatch_item_func_t _Nonnull func)
{
    List_ForEach(&self->workers, ListNode, {
        dispatch_worker_t cwp = (dispatch_worker_t)pCurNode;
        dispatch_item_t ip = _dispatch_worker_find_item(cwp, func);

        if (ip) {
            return ip;
        }
    });

    return NULL;
}


////////////////////////////////////////////////////////////////////////////////
// MARK: Item Cache

dispatch_cacheable_item_t _Nullable _dispatch_acquire_cached_item(dispatch_t _Nonnull _Locked self, size_t nbytes, dispatch_item_func_t func, uint16_t flags)
{
    dispatch_cacheable_item_t pip = NULL;
    dispatch_cacheable_item_t ip = NULL;

    SList_ForEach(&self->item_cache, ListNode, {
        dispatch_cacheable_item_t cip = (dispatch_cacheable_item_t)pCurNode;

        if (cip->size >= nbytes) {
            SList_Remove(&self->item_cache, &pip->super.qe, &cip->super.qe);
            self->item_cache_count--;
            ip = cip;
            break;
        }

        pip = cip;
    });

    if (ip == NULL) {
        ip = malloc(nbytes);
    }

    if (ip) {
        ip->size = nbytes;
        ip->super.func = func;
        ip->super.retireFunc = NULL;
        ip->super.flags = flags;
        ip->super.state = DISPATCH_STATE_IDLE;
        ip->super.reserved = 0;
    }

    return ip;
}

void _dispatch_cache_item(dispatch_t _Nonnull _Locked self, dispatch_cacheable_item_t _Nonnull item)
{
    if (self->item_cache_count >= _DISPATCH_MAX_ITEM_CACHE_COUNT) {
        free(item);
        return;
    }

    SList_InsertBeforeFirst(&self->item_cache, &item->super.qe);
    self->item_cache_count++;
}


////////////////////////////////////////////////////////////////////////////////
// MARK: API

bool _dispatch_isactive(dispatch_t _Nonnull _Locked self)
{
    if (self->signature != _DISPATCH_SIGNATURE) {
        errno = EINVAL;
        return false;
    }
    if (self->state != _DISPATCHER_STATE_ACTIVE) {
        errno = ETERMINATED;
        return false;
    }

    return true;
}

int dispatch_submit(dispatch_t _Nonnull self, dispatch_item_t _Nonnull item)
{
    int r = -1;

    mutex_lock(&self->mutex);
    if (_dispatch_isactive(self)) {
        item->flags &= _DISPATCH_ITEM_PUBLIC_MASK;
        r = _dispatch_submit(self, item);
    }
    mutex_unlock(&self->mutex);
    return r;
}

int dispatch_await(dispatch_t _Nonnull self, dispatch_item_t _Nonnull item)
{
    mutex_lock(&self->mutex);
    const int r = _dispatch_await(self, item);
    mutex_unlock(&self->mutex);
    return r;
}


void _async_adapter_func(dispatch_item_t _Nonnull item)
{
    dispatch_async_item_t async_item = (dispatch_async_item_t)item;

    async_item->func(async_item->arg);
}

int dispatch_async(dispatch_t _Nonnull self, dispatch_async_func_t _Nonnull func, void* _Nullable arg)
{
    int r = -1;

    mutex_lock(&self->mutex);
    if (_dispatch_isactive(self)) {
       dispatch_cacheable_item_t item = _dispatch_acquire_cached_item(self, sizeof(struct dispatch_async_item), _async_adapter_func, _DISPATCH_ITEM_CACHEABLE);
    
        if (item) {
            ((dispatch_async_item_t)item)->func = func;
            ((dispatch_async_item_t)item)->arg = arg;
            r = _dispatch_submit(self, (dispatch_item_t)item);
            if (r != 0) {
                _dispatch_cache_item(self, item);
            }
        }
    }
    mutex_unlock(&self->mutex);

    return r;
}


static void _sync_adapter_func(dispatch_item_t _Nonnull item)
{
    dispatch_sync_item_t sync_item = (dispatch_sync_item_t)item;

    sync_item->result = sync_item->func(sync_item->arg);
}

int dispatch_sync(dispatch_t _Nonnull self, dispatch_sync_func_t _Nonnull func, void* _Nullable arg)
{
    int r = -1;

    mutex_lock(&self->mutex);
    if (_dispatch_isactive(self)) {
        dispatch_cacheable_item_t item = _dispatch_acquire_cached_item(self, sizeof(struct dispatch_sync_item), _sync_adapter_func, _DISPATCH_ITEM_CACHEABLE | DISPATCH_ITEM_AWAITABLE);
    
        if (item) {
            ((dispatch_sync_item_t)item)->func = func;
            ((dispatch_sync_item_t)item)->arg = arg;
            ((dispatch_sync_item_t)item)->result = 0;
            if (_dispatch_submit(self, (dispatch_item_t)item) == 0) {
                r = _dispatch_await(self, (dispatch_item_t)item);
                if (r == 0) {
                    r = ((dispatch_sync_item_t)item)->result;
                }
            }
            _dispatch_cache_item(self, item);
        }
    }
    mutex_unlock(&self->mutex);

    return r;
}


static void _dispatch_do_cancel_item(dispatch_t _Nonnull self, int flags, dispatch_item_t _Nonnull item)
{
    switch (item->state) {
        case DISPATCH_STATE_PENDING:
            item->state = DISPATCH_STATE_CANCELLED;
            
            if ((item->flags & _DISPATCH_ITEM_TIMED) != 0) {
                _dispatch_cancel_timer(self, flags, item);
            }
            else {
                List_ForEach(&self->workers, ListNode, {
                    dispatch_worker_t cwp = (dispatch_worker_t)pCurNode;

                    if (_dispatch_worker_cancel_item(cwp, flags, item)) {
                        break;
                    }
                });
            }
            break;

        case DISPATCH_STATE_EXECUTING:
            item->state = DISPATCH_STATE_CANCELLED;
            break;

        default:
            break;
    }
}

void dispatch_cancel_item(dispatch_t _Nonnull self, int flags, dispatch_item_t _Nonnull item)
{
    mutex_lock(&self->mutex);
    _dispatch_do_cancel_item(self, flags, item);
    mutex_unlock(&self->mutex);
}

void dispatch_cancel(dispatch_t _Nonnull self, int flags, dispatch_item_func_t _Nonnull func)
{
    mutex_lock(&self->mutex);
    dispatch_timer_t timer = _dispatch_find_timer(self, func);
    dispatch_item_t item = (timer) ? timer->item : NULL;

    if (item == NULL) {
        item = _dispatch_find_item(self, func);
    }

    if (item) {
        _dispatch_do_cancel_item(self, flags, item);
    }
    mutex_unlock(&self->mutex);
}


dispatch_t _Nullable dispatch_current(void)
{
    dispatch_worker_t wp = _dispatch_worker_current();
    return (wp) ? wp->owner : NULL;
}

dispatch_item_t _Nullable dispatch_current_item(void)
{
    dispatch_worker_t wp = _dispatch_worker_current();
    // It is safe to access worker.current here without taking the dispatcher
    // lock because (a) the fact we got a worker pointer proofs that the caller
    // is executing in the context of this worker and (b) the only way for the
    // caller to execute in this context is that it is executing in the context
    // of an active item and (c) the worker.context field can not change (is
    // effectively constant) as long as this function here executes because by
    // executing this function we prevent the item context from going away before
    // we're done here.
    return (wp) ? wp->current.item : NULL;
}


int dispatch_priority(dispatch_t _Nonnull self)
{
    mutex_lock(&self->mutex);
    const int r = self->attr.priority;
    mutex_unlock(&self->mutex);
    return r;
}

int dispatch_setpriority(dispatch_t _Nonnull self, int priority)
{
    //XXX implement me
    return ENOTSUP;
}

int dispatch_qos(dispatch_t _Nonnull self)
{
    mutex_lock(&self->mutex);
    const int r = self->attr.qos;
    mutex_unlock(&self->mutex);
    return r;
}

int dispatch_setqos(dispatch_t _Nonnull self, int qos)
{
    //XXX implement me
    return ENOTSUP;
}

void dispatch_concurrency_info(dispatch_t _Nonnull self, dispatch_concurrency_info_t* _Nonnull info)
{
    mutex_lock(&self->mutex);
    info->minimum = self->attr.minConcurrency;
    info->maximum = self->attr.maxConcurrency;
    info->current = self->worker_count;
    mutex_unlock(&self->mutex);
}

int dispatch_name(dispatch_t _Nonnull self, char* _Nonnull buf, size_t buflen)
{
    int r = 0;

    mutex_lock(&self->mutex);
    const size_t len = strlen(self->name);

    if (buflen == 0) {
        r = -1;
        goto out;
    }
    if (buflen < (len + 1)) {
        errno = ERANGE;
        r = -1;
    }
    strcpy(buf, self->name);

out:
    mutex_unlock(&self->mutex);
    return r;
}


static bool _dispatch_prepare_enter_main(void)
{
    dispatch_attr_t attr = DISPATCH_ATTR_INIT_SERIAL_INTERACTIVE;

    if (DISPATCH_MAIN->signature == 0) {
        return _dispatch_init(DISPATCH_MAIN, &attr, _DISPATCH_ADOPT_VCPU);
    }

    return false;
}

static _Noreturn _dispatch_run_main(void)
{
    _dispatch_worker_run((dispatch_worker_t)DISPATCH_MAIN->workers.first);
}

_Noreturn dispatch_enter_main(dispatch_async_func_t _Nonnull func, void* _Nullable arg)
{
    if (_dispatch_prepare_enter_main()) {
        if (!dispatch_async(DISPATCH_MAIN, func, arg)) {
            _dispatch_run_main();
            exit(0);
        }
    }

    abort();
    /* NOT REACHED */
}

void dispatch_enter_main_repeating(int flags, const struct timespec* _Nonnull wtp, const struct timespec* _Nonnull itp, dispatch_async_func_t _Nonnull func, void* _Nullable arg)
{
    if (_dispatch_prepare_enter_main()) {
        if (!dispatch_repeating(DISPATCH_MAIN, flags, wtp, itp, func, arg)) {
            _dispatch_run_main();
            exit(0);
        }
    }

    abort();
    /* NOT REACHED */
}


void dispatch_terminate(dispatch_t _Nonnull self, bool cancel)
{
    mutex_lock(&self->mutex);
    if (self->state == _DISPATCHER_STATE_ACTIVE) {
        self->state = _DISPATCHER_STATE_TERMINATING;

        if (cancel) {
            List_ForEach(&self->workers, ListNode, {
                dispatch_worker_t cwp = (dispatch_worker_t)pCurNode;

                _dispatch_worker_drain(cwp);
            });
        }
        // Timers are drained no matter what
        _dispatch_drain_timers(self);


        // Wake up all workers to inform them about the state change
        _dispatch_wakeup_all_workers(self);
    }
    mutex_unlock(&self->mutex);
}

int dispatch_await_termination(dispatch_t _Nonnull self)
{
    int r;

    mutex_lock(&self->mutex);
    switch (self->state) {
        case _DISPATCHER_STATE_ACTIVE:
            errno = ESRCH;
            int r = -1;
            goto out;

        case _DISPATCHER_STATE_TERMINATED:
            r = 0;
            goto out;
    }

    
    while (self->worker_count > 0) {
        cond_wait(&self->cond, &self->mutex);
    }
    self->state = _DISPATCHER_STATE_TERMINATED;
    r = 0;

out:
    mutex_unlock(&self->mutex);
    return r;
}
