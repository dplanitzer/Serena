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


static volatile dispatch_t _Nullable    g_main_dispatcher;
static struct dispatch      g_main_dispatcher_rec;
static volatile spinlock_t  g_main_lock;    // bss init corresponds to SPINLOCK_INIT


// Expects:
// - 'self' points to sizeof(struct dispatch) 0 bytes
static bool _dispatch_init(dispatch_t _Nonnull self, const dispatch_attr_t* _Nonnull attr)
{
    if (attr->maxConcurrency < 1 || attr->maxConcurrency > INT8_MAX || attr->minConcurrency > attr->maxConcurrency) {
        errno = EINVAL;
        return false;
    }
    if (attr->qos < DISPATCH_QOS_BACKGROUND || attr->qos > DISPATCH_QOS_REALTIME) {
        errno = EINVAL;
        return false;
    }
    if (attr->priority < DISPATCH_PRI_LOWEST || attr->priority > DISPATCH_PRI_HIGHEST) {
        errno = EINVAL;
        return false;
    }


    if (mtx_init(&self->mutex) != 0) {
        return false;
    }

    self->attr = *attr;
    self->state = _DISPATCHER_STATE_ACTIVE;

    if (cnd_init(&self->cond) != 0) {
        return false;
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
        if (_dispatch_init(self, attr)) {
            self->groupid = new_vcpu_groupid();

            for (size_t i = 0; i < attr->minConcurrency; i++) {
                if (_dispatch_acquire_worker_with_ownership(self, _DISPATCH_ACQUIRE_VCPU) != 0) {
                    break;
                }
            }

            if (self->worker_count == attr->minConcurrency) {
                return self;
            }
        }

        dispatch_destroy(self);
    }
    return NULL;
}

int dispatch_destroy(dispatch_t _Nullable self)
{
    if (self && self != g_main_dispatcher) {
        if (self->state < _DISPATCHER_STATE_TERMINATED || !queue_empty(&self->zombie_items)) {
            errno = EBUSY;
            return -1;
        }


        queue_for_each(&self->timer_cache, queue_node_t, {
            free(pCurNode);
        });
        self->timer_cache = QUEUE_INIT;


        queue_for_each(&self->item_cache, queue_node_t, {
            free(pCurNode);
        });
        self->item_cache = QUEUE_INIT;


        if (self->sigtraps) {
            free(self->sigtraps);
            self->sigtraps = NULL;
        }

        self->workers = DEQUE_INIT;
        self->zombie_items = QUEUE_INIT;
        self->timers = QUEUE_INIT;

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
        deque_add_last(&self->workers, &worker->worker_qe);
        self->worker_count++;

        return 0;
    }

    return -1;
}

_Noreturn _dispatch_relinquish_worker(dispatch_t _Nonnull _Locked self, dispatch_worker_t _Nonnull worker)
{
    const int adoption = worker->adoption;

    deque_remove(&self->workers, &worker->worker_qe);
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
    deque_for_each(&self->workers, deque_node_t, {
        dispatch_worker_t cwp = (dispatch_worker_t)pCurNode;

        _dispatch_worker_wakeup(cwp);
    });
}

int _dispatch_ensure_worker_capacity(dispatch_t _Nonnull self, int reason)
{
    bool doCreate = false;

    if (self->worker_count < self->attr.minConcurrency) {
        doCreate = true;
    }
    else if (reason == _DISPATCH_EWC_WORK_ITEM && self->worker_count < self->attr.maxConcurrency) {
        doCreate = true;
    }


    if (doCreate) {
        const int r = _dispatch_acquire_worker_with_ownership(self, _DISPATCH_ACQUIRE_VCPU);

        // Don't make the submit fail outright if we can't allocate a new worker
        // and this was just about adding one more.
        if (r != 0 && self->worker_count == 0) {
            return -1;
        }
    }

    return 0;
}

// Find the worker with the most work, dequeue the first item and return it.
dispatch_item_t _Nullable _dispatch_steal_work_item(dispatch_t _Nonnull self)
{
    dispatch_item_t item = NULL;
    dispatch_worker_t most_busy_wp = NULL;
    size_t most_busy_count = 0;

    deque_for_each(&self->workers, deque_node_t, {
        dispatch_worker_t cwp = (dispatch_worker_t)pCurNode;

        if (cwp->work_count > most_busy_count) {
            most_busy_count = cwp->work_count;
            most_busy_wp = cwp;
        }
    });

    if (most_busy_wp) {
        item = (dispatch_item_t) queue_remove_first(&most_busy_wp->work_queue);
        most_busy_wp->work_count--;
    }

    return item;
}


////////////////////////////////////////////////////////////////////////////////
// MARK: Work Items

static int _dispatch_submit(dispatch_t _Nonnull _Locked self, dispatch_item_t _Nonnull item)
{
    // Allow the submission of an Idle, Finished or Cancelled state item
    if (item->state == DISPATCH_STATE_SCHEDULED || item->state == DISPATCH_STATE_EXECUTING) {
        errno = EBUSY;
        return -1;
    }


    // Ensure that we got enough worker capacity going
    if (_dispatch_ensure_worker_capacity(self, _DISPATCH_EWC_WORK_ITEM) != 0) {
        return -1;
    }


    // Find the worker with the least amount of work scheduled
    dispatch_worker_t best_wp = NULL;
    size_t best_wc = SIZE_MAX;

    deque_for_each(&self->workers, deque_node_t, {
        dispatch_worker_t cwp = (dispatch_worker_t)pCurNode;

        if (cwp->work_count <= best_wc) {
            best_wc = cwp->work_count;
            best_wp = cwp;
        }
    });
    assert(best_wp != NULL);


    // Enqueue the work item at the worker that we found and notify it
    item->qe = QUEUE_NODE_INIT;
    item->state = DISPATCH_STATE_SCHEDULED;
    item->flags &= ~_DISPATCH_ITEM_FLAG_CANCELLED;

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
    while (item->state < DISPATCH_STATE_FINISHED) {
        const int r = cnd_wait(&self->cond, &self->mutex);
        
        if (r != 0) {
            return r;
        }
    }


    bool foundIt = false;
    dispatch_item_t pip = NULL;
    queue_for_each(&self->zombie_items, queue_node_t, {
        dispatch_item_t cip = (dispatch_item_t)pCurNode;

        if (cip == item) {
            foundIt = true;
            break;
        }
        pip = cip;
    });

    
    if (foundIt) {
        if (pip) {
            queue_remove(&self->zombie_items, &pip->qe, &item->qe);
        }
        else {
            queue_remove_first(&self->zombie_items);
        }
    }

    return 0;
}

void _dispatch_zombify_item(dispatch_t _Nonnull _Locked self, dispatch_item_t _Nonnull item)
{
    item->qe = QUEUE_NODE_INIT;
    _queue_add_last(&self->zombie_items, &item->qe);
    cnd_broadcast(&self->cond);
}

static dispatch_item_t _Nullable _dispatch_find_item(dispatch_t _Nonnull self, dispatch_item_func_t _Nonnull func, void* _Nullable arg)
{
    deque_for_each(&self->workers, deque_node_t, {
        dispatch_worker_t cwp = (dispatch_worker_t)pCurNode;
        dispatch_item_t ip = _dispatch_worker_find_item(cwp, func, arg);

        if (ip) {
            return ip;
        }
    });

    return NULL;
}

bool _dispatch_item_has_func(dispatch_item_t _Nonnull item, dispatch_item_func_t _Nonnull func, void* _Nullable arg)
{
    bool hasFunc;
    bool hasArg;

    switch (item->type) {
        case _DISPATCH_TYPE_CONV_ITEM:
        case _DISPATCH_TYPE_CONV_TIMER:
            hasFunc = func == (dispatch_item_func_t)((dispatch_conv_item_t)item)->func;
            hasArg = (arg == DISPATCH_IGNORE_ARG) || (((dispatch_conv_item_t)item)->arg == arg);
            break;

        case _DISPATCH_TYPE_USER_ITEM:
        case _DISPATCH_TYPE_USER_SIGNAL_ITEM:
        case _DISPATCH_TYPE_USER_TIMER:
            hasFunc = func == item->func;
            hasArg = true;
            break;

        default:
            hasFunc = false;
            hasArg = false;
            break;
    }

    return hasFunc && hasArg;
}


////////////////////////////////////////////////////////////////////////////////
// MARK: Item Cache

dispatch_item_t _Nullable _dispatch_acquire_cached_conv_item(dispatch_t _Nonnull _Locked self, dispatch_item_func_t func)
{
    dispatch_item_t ip;

    if (self->item_cache.first) {
        ip = (dispatch_item_t)queue_remove_first(&self->item_cache);
        self->item_cache_count--;
    }
    else {
        ip = malloc(sizeof(struct dispatch_conv_item));
    }

    if (ip) {
        ip->qe = QUEUE_NODE_INIT;
        ip->func = func;
        ip->retireFunc = NULL;
        ip->type = 0;
        ip->subtype = 0;
        ip->flags = 0;
        ip->state = DISPATCH_STATE_IDLE;
    }

    return ip;
}

void _dispatch_cache_item(dispatch_t _Nonnull _Locked self, dispatch_item_t _Nonnull item)
{
    if (self->item_cache_count < _DISPATCH_MAX_CONV_ITEM_CACHE_COUNT) {
        item->qe = QUEUE_NODE_INIT;
        _queue_add_first(&self->item_cache, &item->qe);
        self->item_cache_count++;
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

int dispatch_item_async(dispatch_t _Nonnull self, int flags, dispatch_item_t _Nonnull item)
{
    int r = -1;

    if (item->func == NULL) {
        errno = EINVAL;
        return -1;
    }

    mtx_lock(&self->mutex);
    if (_dispatch_isactive(self)) {
        item->type = _DISPATCH_TYPE_USER_ITEM;
        item->flags = (uint8_t)(flags & _DISPATCH_ITEM_FLAG_AWAITABLE);
        r = _dispatch_submit(self, item);
    }
    mtx_unlock(&self->mutex);
    return r;
}

int dispatch_item_sync(dispatch_t _Nonnull self, dispatch_item_t _Nonnull item)
{
    int r = -1;

    if (item->func == NULL) {
        errno = EINVAL;
        return -1;
    }

    mtx_lock(&self->mutex);
    if (_dispatch_isactive(self)) {
        item->type = _DISPATCH_TYPE_USER_ITEM;
        item->flags = _DISPATCH_ITEM_FLAG_AWAITABLE;
        r = _dispatch_submit(self, item);
        if (r == 0) {
            r = _dispatch_await(self, item);
        }
    }
    mtx_unlock(&self->mutex);
    return r;
}

int dispatch_item_await(dispatch_t _Nonnull self, dispatch_item_t _Nonnull item)
{
    mtx_lock(&self->mutex);
    const int r = _dispatch_await(self, item);
    mtx_unlock(&self->mutex);
    return r;
}


void _async_adapter_func(dispatch_item_t _Nonnull item)
{
    dispatch_conv_item_t ip = (dispatch_conv_item_t)item;

    (void)ip->func(ip->arg);
}

int dispatch_async(dispatch_t _Nonnull self, dispatch_async_func_t _Nonnull func, void* _Nullable arg)
{
    int r = -1;

    mtx_lock(&self->mutex);
    if (_dispatch_isactive(self)) {
       dispatch_conv_item_t item = (dispatch_conv_item_t)_dispatch_acquire_cached_conv_item(self, _async_adapter_func);
    
        if (item) {
            item->super.type = _DISPATCH_TYPE_CONV_ITEM;
            item->super.flags = _DISPATCH_ITEM_FLAG_CACHEABLE;
            item->func = (int (*)(void*))func;
            item->arg = arg;
            r = _dispatch_submit(self, (dispatch_item_t)item);
            if (r != 0) {
                _dispatch_cache_item(self, (dispatch_item_t)item);
            }
        }
    }
    mtx_unlock(&self->mutex);

    return r;
}


static void _sync_adapter_func(dispatch_item_t _Nonnull item)
{
    dispatch_conv_item_t ip = (dispatch_conv_item_t)item;

    ip->result = ip->func(ip->arg);
}

int dispatch_sync(dispatch_t _Nonnull self, dispatch_sync_func_t _Nonnull func, void* _Nullable arg)
{
    int r = -1;

    mtx_lock(&self->mutex);
    if (_dispatch_isactive(self)) {
        dispatch_conv_item_t item = (dispatch_conv_item_t)_dispatch_acquire_cached_conv_item(self, _sync_adapter_func);
    
        if (item) {
            item->super.type = _DISPATCH_TYPE_CONV_ITEM;
            item->super.flags = _DISPATCH_ITEM_FLAG_CACHEABLE | _DISPATCH_ITEM_FLAG_AWAITABLE;
            item->func = (int (*)(void*))func;
            item->arg = arg;
            item->result = 0;
            r = _dispatch_submit(self, (dispatch_item_t)item);
            if (r == 0) {
                r = _dispatch_await(self, (dispatch_item_t)item);
                if (r == 0) {
                    r = item->result;
                }
            }
            _dispatch_cache_item(self, (dispatch_item_t)item);
        }
    }
    mtx_unlock(&self->mutex);

    return r;
}


static void _dispatch_do_cancel_item(dispatch_t _Nonnull self, dispatch_item_t _Nonnull item)
{
    switch (item->state) {
        case DISPATCH_STATE_SCHEDULED:
            item->flags |= _DISPATCH_ITEM_FLAG_CANCELLED;
            
            switch (item->type) {
                case _DISPATCH_TYPE_USER_ITEM:
                case _DISPATCH_TYPE_CONV_ITEM:
                    deque_for_each(&self->workers, deque_node_t, {
                        dispatch_worker_t cwp = (dispatch_worker_t)pCurNode;

                        if (_dispatch_worker_withdraw_item(cwp, item)) {
                            break;
                        }
                    });
                    break;

                case _DISPATCH_TYPE_USER_TIMER:
                case _DISPATCH_TYPE_CONV_TIMER:
                    _dispatch_withdraw_timer_for_item(self, item);
                    break;
                
                case _DISPATCH_TYPE_USER_SIGNAL_ITEM:
                    _dispatch_withdraw_signal_item(self, item);
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

void dispatch_cancel_item(dispatch_t _Nonnull self, dispatch_item_t _Nonnull item)
{
    mtx_lock(&self->mutex);
    _dispatch_do_cancel_item(self, item);
    mtx_unlock(&self->mutex);
}

void dispatch_cancel(dispatch_t _Nonnull self, int flags, dispatch_item_func_t _Nonnull func, void* _Nullable arg)
{
    mtx_lock(&self->mutex);
    dispatch_worker_t wp = _dispatch_worker_current();

    if (wp && wp->current_item && _dispatch_item_has_func(wp->current_item, func, arg)) {
        _dispatch_do_cancel_item(wp->owner, wp->current_item);
    }
    else {
        dispatch_timer_t timer = _dispatch_find_timer(self, func, arg);
        dispatch_item_t item = (timer) ? timer->item : _dispatch_find_item(self, func, arg);

        if (item) {
            _dispatch_do_cancel_item(self, item);
        }
    }
    mtx_unlock(&self->mutex);
}

bool dispatch_current_item_cancelled(void)
{
    dispatch_worker_t wp = _dispatch_worker_current();

    // See dispatch_current_item() why this is safe
    if (wp && wp->current_item) {
        return (wp->current_item->state == DISPATCH_STATE_CANCELLED || (wp->current_item->flags & _DISPATCH_ITEM_FLAG_CANCELLED) != 0) ? true : false;
    }
    else {
        return false;
    }
}

bool dispatch_item_cancelled(dispatch_t _Nonnull self, dispatch_item_t _Nonnull item)
{
    mtx_lock(&self->mutex);
    const bool r = (item->state == DISPATCH_STATE_CANCELLED || (item->flags & _DISPATCH_ITEM_FLAG_CANCELLED) != 0) ? true : false;
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

    deque_for_each(&self->workers, deque_node_t, 
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
        memset(&g_main_dispatcher_rec, 0, sizeof(struct dispatch));
        if (!_dispatch_init(&g_main_dispatcher_rec, &attr)) {
            abort();
        }

        g_main_dispatcher_rec.groupid = vcpu_groupid(vcpu_main());
        if (_dispatch_acquire_worker_with_ownership(&g_main_dispatcher_rec, _DISPATCH_ADOPT_MAIN_VCPU) != 0) {
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

                deque_for_each(&self->workers, deque_node_t, {
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


void dispatch_terminate(dispatch_t _Nonnull self, int flags)
{
    bool isAwaitable = false;

    mtx_lock(&self->mutex);
    if (self != g_main_dispatcher && self->state < _DISPATCHER_STATE_TERMINATING) {
        self->state = _DISPATCHER_STATE_TERMINATING;
        isAwaitable = true;

        if ((flags & DISPATCH_TERMINATE_CANCEL_ALL) == DISPATCH_TERMINATE_CANCEL_ALL) {
            deque_for_each(&self->workers, deque_node_t, {
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


    if (isAwaitable && (flags & DISPATCH_TERMINATE_AWAIT_ALL) == DISPATCH_TERMINATE_AWAIT_ALL) {
        dispatch_await_termination(self);
    }
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
