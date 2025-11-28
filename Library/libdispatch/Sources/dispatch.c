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
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/spinlock.h>


static dispatch_t _Nullable g_main_dispatcher;
static struct dispatch      g_main_dispatcher_rec;
static volatile spinlock_t  g_main_lock;    // bss init corresponds to SPINLOCK_INIT


static bool _dispatch_init(dispatch_t _Nonnull self, const dispatch_attr_t* _Nonnull attr, int adoption)
{
    if (attr->maxConcurrency < 1 || attr->maxConcurrency > INT8_MAX || attr->minConcurrency > attr->maxConcurrency) {
        errno = EINVAL;
        return false;
    }
    if (attr->qos < DISPATCH_QOS_BACKGROUND || attr->qos > DISPATCH_QOS_REALTIME) {
        errno = EINVAL;
        return false;
    }
    if (attr->priority < DISPATCH_PRI_LOWEST || attr->priority >= DISPATCH_PRI_HIGHEST) {
        errno = EINVAL;
        return false;
    }


    memset(self, 0, sizeof(struct dispatch));

    if (mtx_init(&self->mutex) != 0) {
        return false;
    }

    self->attr = *attr;

    switch (adoption) {
        case _DISPATCH_ACQUIRE_VCPU:
            self->groupid = new_vcpu_groupid();
            break;

        case _DISPATCH_ADOPT_CALLER_VCPU:
            self->groupid = vcpu_groupid(vcpu_self());
            break;

        case _DISPATCH_ADOPT_MAIN_VCPU:
            self->groupid = vcpu_groupid(vcpu_main());
            break;

        default:
            abort();
    }

    self->state = _DISPATCHER_STATE_ACTIVE;
    self->item_cache[_DISPATCH_ITEM_CACHE_IDX(_DISPATCH_TYPE_CONV_ITEM)].maxCount = _DISPATCH_MAX_CONV_ITEM_CACHE_COUNT;
    self->item_cache[_DISPATCH_ITEM_CACHE_IDX(_DISPATCH_TYPE_CONV_TIMER)].maxCount = _DISPATCH_MAX_CONV_TIMER_CACHE_COUNT;

    if (cnd_init(&self->cond) != 0) {
        return false;
    }

    for (size_t i = 0; i < attr->minConcurrency; i++) {
        if (_dispatch_acquire_worker_with_ownership(self, adoption) != 0) {
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
    if (self && self != g_main_dispatcher) {
        if (self->state < _DISPATCHER_STATE_TERMINATED || !SList_IsEmpty(&self->zombie_items)) {
            errno = EBUSY;
            return -1;
        }


        SList_ForEach(&self->timer_cache, SListNode, {
            free(pCurNode);
        });
        self->timer_cache = SLIST_INIT;


        for (int i = 0; i < _DISPATCH_ITEM_CACHE_COUNT; i++) {
            SList_ForEach(&self->item_cache[i].items, SListNode, {
                free(pCurNode);
            });
            self->item_cache[i].items = SLIST_INIT;
            self->item_cache[i].count = 0;
        }


        if (self->sigmons) {
            free(self->sigmons);
            self->sigmons = NULL;
        }

        self->workers = LIST_INIT;
        self->zombie_items = SLIST_INIT;
        self->timers = SLIST_INIT;

        cnd_deinit(&self->cond);
        mtx_deinit(&self->mutex);

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
    const int adoption = worker->adoption;

    List_Remove(&self->workers, &worker->worker_qe);
    self->worker_count--;

    _dispatch_worker_destroy(worker);

    cnd_broadcast(&self->cond);
    mtx_unlock(&self->mutex);

    if (adoption == _DISPATCH_ACQUIRE_VCPU) {
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

    // Allow the submission of an Idle, Finished or Cancelled state item
    if (item->state == DISPATCH_STATE_SCHEDULED || item->state == DISPATCH_STATE_EXECUTING) {
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

    item->state = DISPATCH_STATE_SCHEDULED;
    item->qe = SLISTNODE_INIT;


    // Enqueue the work item at the worker that we found and notify it
    _dispatch_worker_submit(best_wp, item, true);

    return 0;
}

void _dispatch_retire_item(dispatch_t _Nonnull _Locked self, dispatch_item_t _Nonnull item)
{
    if ((item->flags & _DISPATCH_ITEM_FLAG_CANCELLED) != 0) {
        item->state = DISPATCH_STATE_CANCELLED;
    }
    else {
        item->state = DISPATCH_STATE_FINISHED;
    }


    if ((item->flags & _DISPATCH_ITEM_FLAG_AWAITABLE) != 0) {
        _dispatch_zombify_item(self, item);
    }
    else if ((item->flags & _DISPATCH_ITEM_FLAG_CACHEABLE) != 0) {
        _dispatch_cache_item(self, item);
    }
    else if (item->retireFunc) {
        item->retireFunc(item);
    }
}

static int _dispatch_await(dispatch_t _Nonnull _Locked self, dispatch_item_t _Nonnull item)
{
    int r = 0;

    while (item->state < DISPATCH_STATE_FINISHED) {
        r = cnd_wait(&self->cond, &self->mutex);
        if (r != 0) {
            break;
        }
    }

    dispatch_item_t pip = NULL;
    SList_ForEach(&self->zombie_items, SListNode, {
        dispatch_item_t cip = (dispatch_item_t)pCurNode;

        if (cip == item) {
            break;
        }
        pip = cip;
    });
    SList_Remove(&self->zombie_items, &pip->qe, &item->qe);

    return r;
}

void _dispatch_zombify_item(dispatch_t _Nonnull _Locked self, dispatch_item_t _Nonnull item)
{
    SList_InsertAfterLast(&self->zombie_items, &item->qe);
    cnd_broadcast(&self->cond);
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

dispatch_item_t _Nullable _dispatch_acquire_cached_item(dispatch_t _Nonnull _Locked self, int itemType, dispatch_item_func_t func)
{
    struct dispatch_item_cache* icp = &self->item_cache[_DISPATCH_ITEM_CACHE_IDX(itemType)];
    dispatch_item_t ip = (dispatch_item_t)SList_RemoveFirst(&icp->items);

    if (ip == NULL) {
        size_t nbytes;

        switch (itemType) {
            case _DISPATCH_TYPE_CONV_ITEM:  nbytes = sizeof(struct dispatch_conv_item); break;
            case _DISPATCH_TYPE_CONV_TIMER: nbytes = sizeof(struct dispatch_conv_timer); break;
            default: abort();
        }
        ip = malloc(nbytes);
    }

    if (ip) {
        ip->qe = SLISTNODE_INIT;
        ip->func = func;
        ip->retireFunc = NULL;
        ip->type = itemType;
        ip->subtype = 0;
        ip->flags = 0;
        ip->state = DISPATCH_STATE_IDLE;
    }

    return ip;
}

void _dispatch_cache_item(dispatch_t _Nonnull _Locked self, dispatch_item_t _Nonnull item)
{
    struct dispatch_item_cache* icp = &self->item_cache[_DISPATCH_ITEM_CACHE_IDX(item->type)];

    if (icp->count < icp->maxCount) {
        item->qe = SLISTNODE_INIT;
        SList_InsertBeforeFirst(&icp->items, &item->qe);
        icp->count++;
    }
    else {
        free(item);
    }
}


////////////////////////////////////////////////////////////////////////////////
// MARK: API

bool _dispatch_isactive(dispatch_t _Nonnull _Locked self)
{
    if (self->state < _DISPATCHER_STATE_TERMINATING) {
        return true;
    }

    errno = ETERMINATED;
    return false;
}

int dispatch_submit(dispatch_t _Nonnull self, int flags, dispatch_item_t _Nonnull item)
{
    int r = -1;

    mtx_lock(&self->mutex);
    if (_dispatch_isactive(self)) {
        item->type = _DISPATCH_TYPE_USER_ITEM;
        item->flags = (uint8_t)(flags & _DISPATCH_ITEM_FLAG_AWAITABLE);
        r = _dispatch_submit(self, item);
    }
    mtx_unlock(&self->mutex);
    return r;
}

int dispatch_await(dispatch_t _Nonnull self, dispatch_item_t _Nonnull item)
{
    mtx_lock(&self->mutex);
    const int r = _dispatch_await(self, item);
    mtx_unlock(&self->mutex);
    return r;
}


static void _async_adapter_func(dispatch_item_t _Nonnull item)
{
    dispatch_conv_item_t ip = (dispatch_conv_item_t)item;

    ip->u.async.func(ip->u.async.arg);
}

int dispatch_async(dispatch_t _Nonnull self, dispatch_async_func_t _Nonnull func, void* _Nullable arg)
{
    int r = -1;

    mtx_lock(&self->mutex);
    if (_dispatch_isactive(self)) {
       dispatch_item_t item = _dispatch_acquire_cached_item(self, _DISPATCH_TYPE_CONV_ITEM, _async_adapter_func);
    
        if (item) {
            ((dispatch_item_t)item)->flags = _DISPATCH_ITEM_FLAG_CACHEABLE;
            ((dispatch_conv_item_t)item)->u.async.func = func;
            ((dispatch_conv_item_t)item)->u.async.arg = arg;
            r = _dispatch_submit(self, (dispatch_item_t)item);
            if (r != 0) {
                _dispatch_cache_item(self, item);
            }
        }
    }
    mtx_unlock(&self->mutex);

    return r;
}


static void _sync_adapter_func(dispatch_item_t _Nonnull item)
{
    dispatch_conv_item_t ip = (dispatch_conv_item_t)item;

    ip->u.sync.result = ip->u.sync.func(ip->u.sync.arg);
}

int dispatch_sync(dispatch_t _Nonnull self, dispatch_sync_func_t _Nonnull func, void* _Nullable arg)
{
    int r = -1;

    mtx_lock(&self->mutex);
    if (_dispatch_isactive(self)) {
        dispatch_item_t item = _dispatch_acquire_cached_item(self, _DISPATCH_TYPE_CONV_ITEM, _sync_adapter_func);
    
        if (item) {
            ((dispatch_item_t)item)->flags = _DISPATCH_ITEM_FLAG_CACHEABLE | _DISPATCH_ITEM_FLAG_AWAITABLE;
            ((dispatch_conv_item_t)item)->u.sync.func = func;
            ((dispatch_conv_item_t)item)->u.sync.arg = arg;
            ((dispatch_conv_item_t)item)->u.sync.result = 0;
            if (_dispatch_submit(self, (dispatch_item_t)item) == 0) {
                r = _dispatch_await(self, (dispatch_item_t)item);
                if (r == 0) {
                    r = ((dispatch_conv_item_t)item)->u.sync.result;
                }
            }
            _dispatch_cache_item(self, item);
        }
    }
    mtx_unlock(&self->mutex);

    return r;
}


static void _dispatch_do_cancel_item(dispatch_t _Nonnull self, int flags, dispatch_item_t _Nonnull item)
{
    switch (item->state) {
        case DISPATCH_STATE_SCHEDULED:
            item->flags |= _DISPATCH_ITEM_FLAG_CANCELLED;
            
            switch (item->type) {
                case _DISPATCH_TYPE_USER_ITEM:
                case _DISPATCH_TYPE_CONV_ITEM:
                    List_ForEach(&self->workers, ListNode, {
                        dispatch_worker_t cwp = (dispatch_worker_t)pCurNode;

                        if (_dispatch_worker_withdraw_item(cwp, flags, item)) {
                            break;
                        }
                    });
                    break;

                case _DISPATCH_TYPE_USER_TIMER:
                case _DISPATCH_TYPE_CONV_TIMER:
                    _dispatch_withdraw_timer(self, flags, item);
                    break;
                
                case _DISPATCH_TYPE_USER_SIGNAL_ITEM:
                    _dispatch_withdraw_signal_item(self, flags, item);
                    break;

                default:
                    abort();
            }
            break;

        case DISPATCH_STATE_EXECUTING:
            item->flags |= _DISPATCH_ITEM_FLAG_CANCELLED;
            break;

        default:
            break;
    }
}

void dispatch_cancel_item(dispatch_t _Nonnull self, int flags, dispatch_item_t _Nonnull item)
{
    mtx_lock(&self->mutex);
    _dispatch_do_cancel_item(self, flags, item);
    mtx_unlock(&self->mutex);
}

void dispatch_cancel(dispatch_t _Nonnull self, int flags, dispatch_item_func_t _Nonnull func)
{
    mtx_lock(&self->mutex);
    dispatch_item_t item = (dispatch_item_t)_dispatch_find_timer(self, func);

    if (item == NULL) {
        item = _dispatch_find_item(self, func);
    }

    if (item) {
        _dispatch_do_cancel_item(self, flags, item);
    }
    mtx_unlock(&self->mutex);
}

void dispatch_cancel_current_item(int flags)
{
    dispatch_worker_t wp = _dispatch_worker_current();

    if (wp && wp->current_item) {
        dispatch_cancel_item(wp->owner, flags, wp->current_item);
    }
}

bool dispatch_item_cancelled(dispatch_t _Nonnull self, dispatch_item_t _Nonnull item)
{
    mtx_lock(&self->mutex);
    const bool r = (item->state == DISPATCH_STATE_CANCELLED) ? true : false;
    mtx_unlock(&self->mutex);
    return r;
}


dispatch_t _Nullable dispatch_current_queue(void)
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
    return (wp) ? wp->current_item : NULL;
}


static void _dispatch_applyschedparams(dispatch_t _Nonnull _Locked self, int qos, int priority)
{
    sched_params_t params;

    params.type = SCHED_PARAM_QOS;
    params.u.qos.category = qos;
    params.u.qos.priority = priority;
    
    self->attr.qos = qos;
    self->attr.priority = priority;

    List_ForEach(&self->workers, ListNode, 
        dispatch_worker_t cwp = (dispatch_worker_t)pCurNode;

        vcpu_setschedparams(cwp->vcpu, &params);
    );
}

int dispatch_priority(dispatch_t _Nonnull self)
{
    mtx_lock(&self->mutex);
    const int r = self->attr.priority;
    mtx_unlock(&self->mutex);
    return r;
}

int dispatch_setpriority(dispatch_t _Nonnull self, int priority)
{
    if (priority < DISPATCH_PRI_LOWEST || priority > DISPATCH_PRI_HIGHEST) {
        errno = EINVAL;
        return -1;
    }

    mtx_lock(&self->mutex);
    _dispatch_applyschedparams(self, self->attr.qos, priority);
    mtx_unlock(&self->mutex);
    
    return 0;
}

int dispatch_qos(dispatch_t _Nonnull self)
{
    mtx_lock(&self->mutex);
    const int r = self->attr.qos;
    mtx_unlock(&self->mutex);
    return r;
}

int dispatch_setqos(dispatch_t _Nonnull self, int qos)
{
    if (qos < DISPATCH_QOS_BACKGROUND || qos > DISPATCH_QOS_REALTIME) {
        errno = EINVAL;
        return -1;
    }

    mtx_lock(&self->mutex);
    _dispatch_applyschedparams(self, qos, self->attr.priority);
    mtx_unlock(&self->mutex);
    
    return 0;
}

void dispatch_concurrency_info(dispatch_t _Nonnull self, dispatch_concurrency_info_t* _Nonnull info)
{
    mtx_lock(&self->mutex);
    info->minimum = self->attr.minConcurrency;
    info->maximum = self->attr.maxConcurrency;
    info->current = self->worker_count;
    mtx_unlock(&self->mutex);
}

int dispatch_name(dispatch_t _Nonnull self, char* _Nonnull buf, size_t buflen)
{
    int r = 0;

    mtx_lock(&self->mutex);
    const size_t len = strlen(self->name);

    if (buflen == 0) {
        errno = EINVAL;
        r = -1;
        goto out;
    }
    if (buflen < (len + 1)) {
        errno = ERANGE;
        r = -1;
        goto out;
    }
    strcpy(buf, self->name);

out:
    mtx_unlock(&self->mutex);
    return r;
}


dispatch_t _Nonnull dispatch_main_queue(void)
{
    dispatch_attr_t attr = DISPATCH_ATTR_INIT_SERIAL_INTERACTIVE;
    dispatch_t p = NULL;

    // spinlock: it's fine because there's virtually no contention on this lock
    //           once the main dispatcher has been allocated. There shouldn't be
    //           any contention on the lock while we're allocating the dispatcher
    //           because nobody besides the main vcpu should be in here at that
    //           time.
    // _DISPATCH_ADOPT_MAIN_VCPU: theoretically this may be called from some
    //           secondary vcpu before the main vcpu gets a chance to set things
    //           up.
    spin_lock(&g_main_lock);
    if (g_main_dispatcher == NULL) {
        if (!_dispatch_init(&g_main_dispatcher_rec, &attr, _DISPATCH_ADOPT_MAIN_VCPU)) {
            abort();
        }

        g_main_dispatcher = &g_main_dispatcher_rec;
    }

    p = g_main_dispatcher;
    spin_unlock(&g_main_lock);

    return p;
}

_Noreturn dispatch_run_main_queue(void)
{
    if (vcpu_self() != vcpu_main()) {
        abort();
    }

    _dispatch_worker_run((dispatch_worker_t)dispatch_main_queue()->workers.first);
}


int dispatch_suspend(dispatch_t _Nonnull self)
{
    int r;

    mtx_lock(&self->mutex);

    if (_dispatch_isactive(self)) {
        self->suspension_count++;
        if (self->suspension_count == 1) {
            if (self->state == _DISPATCHER_STATE_ACTIVE) {
                self->state = _DISPATCHER_STATE_SUSPENDING;
            }


            // Wait for all workers to have reached suspended state before we
            // switch the dispatcher to suspended state
            for (;;) {
                bool hasStillActiveWorker = false;

                List_ForEach(&self->workers, ListNode, {
                    dispatch_worker_t cwp = (dispatch_worker_t)pCurNode;

                    if (!cwp->is_suspended) {
                        hasStillActiveWorker = true;
                        break;
                    }
                });

                if (!hasStillActiveWorker) {
                    self->state = _DISPATCHER_STATE_SUSPENDED;
                    break;
                }

                cnd_wait(&self->cond, &self->mutex);
            }
        }
        r = 0;
    }
    else {
        r = -1;
    }

    mtx_unlock(&self->mutex);
    return r;
}

void dispatch_resume(dispatch_t _Nonnull self)
{
    mtx_lock(&self->mutex);

    if (_dispatch_isactive(self)) {
        if (self->suspension_count > 0) {
            self->suspension_count--;
            if (self->suspension_count == 0) {
                self->state = _DISPATCHER_STATE_ACTIVE;
                _dispatch_wakeup_all_workers(self);
            }
        }
    }

    mtx_unlock(&self->mutex);
}


void dispatch_terminate(dispatch_t _Nonnull self, bool cancel)
{
    mtx_lock(&self->mutex);
    if (self != g_main_dispatcher && self->state < _DISPATCHER_STATE_TERMINATING) {
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
    mtx_unlock(&self->mutex);
}

int dispatch_await_termination(dispatch_t _Nonnull self)
{
    int r;

    mtx_lock(&self->mutex);
    switch (self->state) {
        case _DISPATCHER_STATE_ACTIVE:
        case _DISPATCHER_STATE_SUSPENDING:
        case _DISPATCHER_STATE_SUSPENDED:
            errno = ESRCH;
            r = -1;
            break;

        case _DISPATCHER_STATE_TERMINATING:
            while (self->worker_count > 0) {
                cnd_wait(&self->cond, &self->mutex);
            }
            self->state = _DISPATCHER_STATE_TERMINATED;
            r = 0;
            break;

        case _DISPATCHER_STATE_TERMINATED:
            r = 0;
            break;

        default:
            abort();
    }
    mtx_unlock(&self->mutex);
    return r;
}
