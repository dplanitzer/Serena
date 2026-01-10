//
//  kdispatch.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/10/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "kdispatch_priv.h"
#include <string.h>
#include <process/Process.h>


// Expects:
// - 'self' points to sizeof(struct kdispatch) 0 bytes
static errno_t _kdispatch_init(kdispatch_t _Nonnull self, const kdispatch_attr_t* _Nonnull attr)
{
    decl_try_err();

    if (attr->maxConcurrency < 1 || attr->maxConcurrency > INT8_MAX || attr->minConcurrency > attr->maxConcurrency) {
        return EINVAL;
    }
    if (attr->qos < KDISPATCH_QOS_BACKGROUND || attr->qos > KDISPATCH_QOS_REALTIME) {
        return EINVAL;
    }
    if (attr->priority < KDISPATCH_PRI_LOWEST || attr->priority > KDISPATCH_PRI_HIGHEST) {
        return EINVAL;
    }


    mtx_init(&self->mutex);

    self->attr = *attr;
    self->groupid = new_vcpu_groupid();
    self->state = _DISPATCHER_STATE_ACTIVE;

    cnd_init(&self->cond);

    for (size_t i = 0; i < attr->minConcurrency; i++) {
        err = _kdispatch_acquire_worker(self);
        if (err != EOK) {
            return err;
        }
    }

    if (attr->name && attr->name[0] != '\0') {
        strncpy(self->name, attr->name, KDISPATCH_MAX_NAME_LENGTH);
    }

    return EOK;
}

errno_t kdispatch_create(const kdispatch_attr_t* _Nonnull attr, kdispatch_t _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    kdispatch_t self = NULL;
    
    err = kalloc_cleared(sizeof(struct kdispatch), (void**)&self);
    if (err == EOK) {
        err = _kdispatch_init(self, attr);
        if (err != EOK) {
            kfree(self);
            self = NULL;
        }
    }

    *pOutSelf = self;
    return err;
}

errno_t kdispatch_destroy(kdispatch_t _Nullable self)
{
    if (self) {
        if (self->state < _DISPATCHER_STATE_TERMINATED || !queue_empty(&self->zombie_items)) {
            return EBUSY;
        }


        queue_for_each(&self->timer_cache, queue_node_t, it,
            kfree(it);
        )
        self->timer_cache = QUEUE_INIT;


        queue_for_each(&self->item_cache, queue_node_t, it,
            kfree(it);
        )
        self->item_cache = QUEUE_INIT;


        if (self->sigtraps) {
            kfree(self->sigtraps);
            self->sigtraps = NULL;
        }

        self->workers = DEQUE_INIT;
        self->zombie_items = QUEUE_INIT;
        self->timers = QUEUE_INIT;

        cnd_deinit(&self->cond);
        mtx_deinit(&self->mutex);

        kfree(self);
    }

    return 0;
}


////////////////////////////////////////////////////////////////////////////////
// MARK: Workers

errno_t _kdispatch_acquire_worker(kdispatch_t _Nonnull _Locked self)
{
    decl_try_err();
    kdispatch_worker_t worker;
    
    err = _kdispatch_worker_create(self, &worker);
    if (err != EOK) {
        return err;
    }

    deque_add_last(&self->workers, &worker->worker_qe);
    self->worker_count++;

    return EOK;
}

_Noreturn _kdispatch_relinquish_worker(kdispatch_t _Nonnull _Locked self, kdispatch_worker_t _Nonnull worker)
{
    vcpu_t vp = vcpu_current();

    deque_remove(&self->workers, &worker->worker_qe);
    self->worker_count--;
    vp->dispatch_worker = NULL;

    _kdispatch_worker_destroy(worker);

    cnd_broadcast(&self->cond);
    mtx_unlock(&self->mutex);

    Process_RelinquishVirtualProcessor(gKernelProcess, vp);
    /* NO RETURN */
}

void _kdispatch_wakeup_all_workers(kdispatch_t _Nonnull self)
{
    deque_for_each(&self->workers, deque_node_t, it,
        kdispatch_worker_t cwp = (kdispatch_worker_t)it;

        _kdispatch_worker_wakeup(cwp);
    )
}

errno_t _kdispatch_ensure_worker_capacity(kdispatch_t _Nonnull self, int reason)
{
    bool doCreate = false;

    if (self->worker_count < self->attr.minConcurrency) {
        doCreate = true;
    }
    else if (reason == _KDISPATCH_EWC_WORK_ITEM && self->worker_count < self->attr.maxConcurrency) {
        doCreate = true;
    }


    if (doCreate) {
        const errno_t err = _kdispatch_acquire_worker(self);

        // Don't make the submit fail outright if we can't allocate a new worker
        // and this was just about adding one more.
        if (err != EOK && self->worker_count == 0) {
            return err;
        }
    }

    return EOK;
}

// Find the worker with the most work, dequeue the first item and return it.
kdispatch_item_t _Nullable _kdispatch_steal_work_item(kdispatch_t _Nonnull self)
{
    kdispatch_item_t item = NULL;
    kdispatch_worker_t most_busy_wp = NULL;
    size_t most_busy_count = 0;

    deque_for_each(&self->workers, deque_node_t, it,
        kdispatch_worker_t cwp = (kdispatch_worker_t)it;

        if (cwp->work_count > most_busy_count) {
            most_busy_count = cwp->work_count;
            most_busy_wp = cwp;
        }
    )

    if (most_busy_wp) {
        item = (kdispatch_item_t) queue_remove_first(&most_busy_wp->work_queue);
        most_busy_wp->work_count--;
    }

    return item;
}


////////////////////////////////////////////////////////////////////////////////
// MARK: Work Items

static errno_t _kdispatch_submit(kdispatch_t _Nonnull _Locked self, kdispatch_item_t _Nonnull item)
{
    decl_try_err();

    // Allow the submission of an Idle, Finished or Cancelled state item
    if (item->state == KDISPATCH_STATE_SCHEDULED || item->state == KDISPATCH_STATE_EXECUTING) {
        return EBUSY;
    }


    // Ensure that we got enough worker capacity going
    if ((err = _kdispatch_ensure_worker_capacity(self, _KDISPATCH_EWC_WORK_ITEM)) != EOK) {
        return err;
    }


    // Find the worker with the least amount of work scheduled
    kdispatch_worker_t best_wp = NULL;
    size_t best_wc = SIZE_MAX;

    deque_for_each(&self->workers, deque_node_t, it,
        kdispatch_worker_t cwp = (kdispatch_worker_t)it;

        if (cwp->work_count <= best_wc) {
            best_wc = cwp->work_count;
            best_wp = cwp;
        }
    )
    assert(best_wp != NULL);


    // Enqueue the work item at the worker that we found and notify it
    item->qe = QUEUE_NODE_INIT;
    item->state = KDISPATCH_STATE_SCHEDULED;
    item->flags &= ~_KDISPATCH_ITEM_FLAG_CANCELLED;

    _kdispatch_worker_submit(best_wp, item, true);

    return EOK;
}

void _kdispatch_retire_item(kdispatch_t _Nonnull _Locked self, kdispatch_item_t _Nonnull item)
{
    if ((item->flags & _KDISPATCH_ITEM_FLAG_CANCELLED) != 0) {
        item->state = KDISPATCH_STATE_CANCELLED;
    }
    else {
        item->state = KDISPATCH_STATE_FINISHED;
    }


    if ((item->flags & _KDISPATCH_ITEM_FLAG_AWAITABLE) != 0) {
        _kdispatch_zombify_item(self, item);
    }
    else if ((item->flags & _KDISPATCH_ITEM_FLAG_CACHEABLE) != 0) {
        _kdispatch_cache_item(self, item);
    }
    else if (item->retireFunc) {
        item->retireFunc(item);
    }
}

static errno_t _kdispatch_await(kdispatch_t _Nonnull _Locked self, kdispatch_item_t _Nonnull item)
{
    while (item->state < KDISPATCH_STATE_FINISHED) {
        const errno_t err = cnd_wait(&self->cond, &self->mutex);
        
        if (err != EOK) {
            return err;
        }
    }


    bool foundIt = false;
    kdispatch_item_t pip = NULL;
    queue_for_each(&self->zombie_items, queue_node_t, it,
        kdispatch_item_t cip = (kdispatch_item_t)it;

        if (cip == item) {
            foundIt = true;
            break;
        }
        pip = cip;
    )


    if (foundIt) {
        if (pip) {
            queue_remove(&self->zombie_items, &pip->qe, &item->qe);
        }
        else {
            _queue_remove_first(&self->zombie_items);
        }
    }

    return EOK;
}

void _kdispatch_zombify_item(kdispatch_t _Nonnull _Locked self, kdispatch_item_t _Nonnull item)
{
    item->qe = QUEUE_NODE_INIT;
    _queue_add_last(&self->zombie_items, &item->qe);
    cnd_broadcast(&self->cond);
}

static kdispatch_item_t _Nullable _kdispatch_find_item(kdispatch_t _Nonnull self, kdispatch_item_func_t _Nonnull func, void* _Nullable arg)
{
    deque_for_each(&self->workers, deque_node_t, it,
        kdispatch_worker_t cwp = (kdispatch_worker_t)it;
        kdispatch_item_t ip = _kdispatch_worker_find_item(cwp, func, arg);

        if (ip) {
            return ip;
        }
    )

    return NULL;
}

bool _kdispatch_item_has_func(kdispatch_item_t _Nonnull item, kdispatch_item_func_t _Nonnull func, void* _Nullable arg)
{
    bool hasFunc;
    bool hasArg;

    switch (item->type) {
        case _KDISPATCH_TYPE_CONV_ITEM:
        case _KDISPATCH_TYPE_CONV_TIMER:
            hasFunc = func == (kdispatch_item_func_t)((kdispatch_conv_item_t)item)->func;
            hasArg = (arg == KDISPATCH_IGNORE_ARG) || (((kdispatch_conv_item_t)item)->arg == arg);
            break;

        case _KDISPATCH_TYPE_USER_ITEM:
        case _KDISPATCH_TYPE_USER_SIGNAL_ITEM:
        case _KDISPATCH_TYPE_USER_TIMER:
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

kdispatch_item_t _Nullable _kdispatch_acquire_cached_conv_item(kdispatch_t _Nonnull _Locked self, kdispatch_item_func_t func)
{
    kdispatch_item_t ip;

    if (self->item_cache.first) {
        ip = (kdispatch_item_t)self->item_cache.first;
        _queue_remove_first(&self->item_cache);
        self->item_cache_count--;
    }
    else {
        kalloc(sizeof(struct kdispatch_conv_item), (void**)&ip);
    }

    if (ip) {
        ip->qe = QUEUE_NODE_INIT;
        ip->func = func;
        ip->retireFunc = NULL;
        ip->type = 0;
        ip->subtype = 0;
        ip->flags = 0;
        ip->state = KDISPATCH_STATE_IDLE;
    }

    return ip;
}

void _kdispatch_cache_item(kdispatch_t _Nonnull _Locked self, kdispatch_item_t _Nonnull item)
{
    if (self->item_cache_count < _KDISPATCH_MAX_CONV_ITEM_CACHE_COUNT) {
        item->qe = QUEUE_NODE_INIT;
        _queue_add_first(&self->item_cache, &item->qe);
        self->item_cache_count++;
    }
    else {
        kfree(item);
    }
}


////////////////////////////////////////////////////////////////////////////////
// MARK: API

errno_t kdispatch_item_async(kdispatch_t _Nonnull self, int flags, kdispatch_item_t _Nonnull item)
{
    decl_try_err();

    if (item->func == NULL) {
        return EINVAL;
    }

    mtx_lock(&self->mutex);
    if (self->state < _DISPATCHER_STATE_TERMINATING) {
        item->type = _KDISPATCH_TYPE_USER_ITEM;
        item->flags = (uint8_t)(flags & _KDISPATCH_ITEM_FLAG_AWAITABLE);
        err = _kdispatch_submit(self, item);
    }
    else {
        err = ETERMINATED;
    }
    mtx_unlock(&self->mutex);
    return err;
}

errno_t kdispatch_item_sync(kdispatch_t _Nonnull self, kdispatch_item_t _Nonnull item)
{
    decl_try_err();

    if (item->func == NULL) {
        return EINVAL;
    }

    mtx_lock(&self->mutex);
    if (self->state < _DISPATCHER_STATE_TERMINATING) {
        item->type = _KDISPATCH_TYPE_USER_ITEM;
        item->flags = _KDISPATCH_ITEM_FLAG_AWAITABLE;
        err = _kdispatch_submit(self, item);
        if (err == EOK) {
            err = _kdispatch_await(self, item);
        }
    }
    else {
        err = ETERMINATED;
    }
    mtx_unlock(&self->mutex);

    return err;
}

errno_t kdispatch_item_await(kdispatch_t _Nonnull self, kdispatch_item_t _Nonnull item)
{
    decl_try_err();

    mtx_lock(&self->mutex);
    err = _kdispatch_await(self, item);
    mtx_unlock(&self->mutex);
    return err;
}


void _async_adapter_func(kdispatch_item_t _Nonnull item)
{
    kdispatch_conv_item_t ip = (kdispatch_conv_item_t)item;

    (void)ip->func(ip->arg);
}

errno_t kdispatch_async(kdispatch_t _Nonnull self, kdispatch_async_func_t _Nonnull func, void* _Nullable arg)
{
    decl_try_err();

    mtx_lock(&self->mutex);
    if (self->state < _DISPATCHER_STATE_TERMINATING) {
       kdispatch_conv_item_t item = (kdispatch_conv_item_t)_kdispatch_acquire_cached_conv_item(self, _async_adapter_func);
    
        if (item) {
            item->super.type = _KDISPATCH_TYPE_CONV_ITEM;
            item->super.flags = _KDISPATCH_ITEM_FLAG_CACHEABLE;
            item->func = (int (*)(void*))func;
            item->arg = arg;
            err = _kdispatch_submit(self, (kdispatch_item_t)item);
            if (err != EOK) {
                _kdispatch_cache_item(self, (kdispatch_item_t)item);
            }
        }
        else {
            err = ENOMEM;
        }
    }
    else {
        err = ETERMINATED;
    }
    mtx_unlock(&self->mutex);

    return err;
}


static void _sync_adapter_func(kdispatch_item_t _Nonnull item)
{
    kdispatch_conv_item_t ip = (kdispatch_conv_item_t)item;

    ip->result = ip->func(ip->arg);
}

errno_t kdispatch_sync(kdispatch_t _Nonnull self, kdispatch_sync_func_t _Nonnull func, void* _Nullable arg)
{
    decl_try_err();

    mtx_lock(&self->mutex);
    if (self->state < _DISPATCHER_STATE_TERMINATING) {
        kdispatch_conv_item_t item = (kdispatch_conv_item_t)_kdispatch_acquire_cached_conv_item(self, _sync_adapter_func);
    
        if (item) {
            item->super.type = _KDISPATCH_TYPE_CONV_ITEM;
            item->super.flags = _KDISPATCH_ITEM_FLAG_CACHEABLE | _KDISPATCH_ITEM_FLAG_AWAITABLE;
            item->func = (int (*)(void*))func;
            item->arg = arg;
            item->result = 0;
            err = _kdispatch_submit(self, (kdispatch_item_t)item);
            if (err == EOK) {
                err = _kdispatch_await(self, (kdispatch_item_t)item);
                if (err == EOK) {
                    err = item->result;
                }
            }
            _kdispatch_cache_item(self, (kdispatch_item_t)item);
        }
        else {
            err = ENOMEM;
        }
    }
    else {
        err = ETERMINATED;
    }
    mtx_unlock(&self->mutex);

    return err;
}


static void _kdispatch_do_cancel_item(kdispatch_t _Nonnull self, kdispatch_item_t _Nonnull item)
{
    switch (item->state) {
        case KDISPATCH_STATE_SCHEDULED:
            item->flags |= _KDISPATCH_ITEM_FLAG_CANCELLED;
            
            switch (item->type) {
                case _KDISPATCH_TYPE_USER_ITEM:
                case _KDISPATCH_TYPE_CONV_ITEM:
                    deque_for_each(&self->workers, deque_node_t, it,
                        kdispatch_worker_t cwp = (kdispatch_worker_t)it;

                        if (_kdispatch_worker_withdraw_item(cwp, item)) {
                            break;
                        }
                    )
                    break;

                case _KDISPATCH_TYPE_USER_TIMER:
                case _KDISPATCH_TYPE_CONV_TIMER:
                    _kdispatch_withdraw_timer_for_item(self, item);
                    break;
                
                case _KDISPATCH_TYPE_USER_SIGNAL_ITEM:
                    _kdispatch_withdraw_signal_item(self, item);
                    break;

                default:
                    abort();
            }
            break;

        case KDISPATCH_STATE_EXECUTING:
            item->flags |= _KDISPATCH_ITEM_FLAG_CANCELLED;
            break;

        default:
            break;
    }
}

void kdispatch_cancel_item(kdispatch_t _Nonnull self, kdispatch_item_t _Nonnull item)
{
    mtx_lock(&self->mutex);
    _kdispatch_do_cancel_item(self, item);
    mtx_unlock(&self->mutex);
}

void kdispatch_cancel(kdispatch_t _Nonnull self, int flags, kdispatch_item_func_t _Nonnull func, void* _Nullable arg)
{
    mtx_lock(&self->mutex);
    kdispatch_worker_t wp = _kdispatch_worker_current();

    if (wp && wp->current_item && _kdispatch_item_has_func(wp->current_item, func, arg)) {
        _kdispatch_do_cancel_item(wp->owner, wp->current_item);
    }
    else {
        kdispatch_timer_t timer = _kdispatch_find_timer(self, func, arg);
        kdispatch_item_t item = (timer) ? timer->item : _kdispatch_find_item(self, func, arg);

        if (item) {
            _kdispatch_do_cancel_item(self, item);
        }
    }
    mtx_unlock(&self->mutex);
}

bool kdispatch_current_item_cancelled(void)
{
    kdispatch_worker_t wp = _kdispatch_worker_current();

    // See dispatch_current_item() why this is safe
    if (wp && wp->current_item) {
        return (wp->current_item->state == KDISPATCH_STATE_CANCELLED || (wp->current_item->flags & _KDISPATCH_ITEM_FLAG_CANCELLED) != 0) ? true : false;
    }
    else {
        return false;
    }
}

bool kdispatch_item_cancelled(kdispatch_t _Nonnull self, kdispatch_item_t _Nonnull item)
{
    mtx_lock(&self->mutex);
    const bool r = (item->state == KDISPATCH_STATE_CANCELLED) ? true : false;
    mtx_unlock(&self->mutex);
    return r;
}


kdispatch_t _Nullable kdispatch_current_queue(void)
{
    kdispatch_worker_t wp = _kdispatch_worker_current();
    return (wp) ? wp->owner : NULL;
}

kdispatch_item_t _Nullable kdispatch_current_item(void)
{
    kdispatch_worker_t wp = _kdispatch_worker_current();
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


static void _kdispatch_applyschedparams(kdispatch_t _Nonnull _Locked self, int qos, int priority)
{
    sched_params_t params;

    params.type = SCHED_PARAM_QOS;
    params.u.qos.category = qos;
    params.u.qos.priority = priority;
    
    self->attr.qos = qos;
    self->attr.priority = priority;

    deque_for_each(&self->workers, deque_node_t, it,
        kdispatch_worker_t cwp = (kdispatch_worker_t)it;

        vcpu_setschedparams(cwp->vcpu, &params);
    )
}

int kdispatch_priority(kdispatch_t _Nonnull self)
{
    mtx_lock(&self->mutex);
    const int r = self->attr.priority;
    mtx_unlock(&self->mutex);
    return r;
}

errno_t kdispatch_setpriority(kdispatch_t _Nonnull self, int priority)
{
    if (priority < KDISPATCH_PRI_LOWEST || priority > KDISPATCH_PRI_HIGHEST) {
        return EINVAL;
    }

    mtx_lock(&self->mutex);
    _kdispatch_applyschedparams(self, self->attr.qos, priority);
    mtx_unlock(&self->mutex);
    
    return EOK;
}

int kdispatch_qos(kdispatch_t _Nonnull self)
{
    mtx_lock(&self->mutex);
    const int r = self->attr.qos;
    mtx_unlock(&self->mutex);
    return r;
}

errno_t kdispatch_setqos(kdispatch_t _Nonnull self, int qos)
{
    if (qos < KDISPATCH_QOS_BACKGROUND || qos > KDISPATCH_QOS_REALTIME) {
        return EINVAL;
    }

    mtx_lock(&self->mutex);
    _kdispatch_applyschedparams(self, qos, self->attr.priority);
    mtx_unlock(&self->mutex);
    
    return EOK;
}

void kdispatch_concurrency_info(kdispatch_t _Nonnull self, kdispatch_concurrency_info_t* _Nonnull info)
{
    mtx_lock(&self->mutex);
    info->minimum = self->attr.minConcurrency;
    info->maximum = self->attr.maxConcurrency;
    info->current = self->worker_count;
    mtx_unlock(&self->mutex);
}

errno_t kdispatch_name(kdispatch_t _Nonnull self, char* _Nonnull buf, size_t buflen)
{
    decl_try_err();

    mtx_lock(&self->mutex);
    const size_t len = strlen(self->name);

    if (buflen == 0) {
        throw(EINVAL);
    }
    if (buflen < (len + 1)) {
        throw(ERANGE);
    }
    strcpy(buf, self->name);

catch:
    mtx_unlock(&self->mutex);
    return err;
}


errno_t kdispatch_suspend(kdispatch_t _Nonnull self)
{
    decl_try_err();

    mtx_lock(&self->mutex);

    if (self->state < _DISPATCHER_STATE_TERMINATING) {
        self->suspension_count++;
        if (self->suspension_count == 1) {
            if (self->state == _DISPATCHER_STATE_ACTIVE) {
                self->state = _DISPATCHER_STATE_SUSPENDING;
            }


            // Wait for all workers to have reached suspended state before we
            // switch the dispatcher to suspended state
            for (;;) {
                bool hasStillActiveWorker = false;

                deque_for_each(&self->workers, deque_node_t, it,
                    kdispatch_worker_t cwp = (kdispatch_worker_t)it;

                    if (!cwp->is_suspended) {
                        hasStillActiveWorker = true;
                        break;
                    }
                )

                if (!hasStillActiveWorker) {
                    self->state = _DISPATCHER_STATE_SUSPENDED;
                    break;
                }

                cnd_wait(&self->cond, &self->mutex);
            }
        }
    }
    else {
        err = ETERMINATED;
    }

    mtx_unlock(&self->mutex);
    return err;
}

void kdispatch_resume(kdispatch_t _Nonnull self)
{
    mtx_lock(&self->mutex);

    if (self->state < _DISPATCHER_STATE_TERMINATING) {
        if (self->suspension_count > 0) {
            self->suspension_count--;
            if (self->suspension_count == 0) {
                self->state = _DISPATCHER_STATE_ACTIVE;
                _kdispatch_wakeup_all_workers(self);
            }
        }
    }

    mtx_unlock(&self->mutex);
}


void kdispatch_terminate(kdispatch_t _Nonnull self, int flags)
{
    bool isAwaitable = false;

    mtx_lock(&self->mutex);
    if (self->state < _DISPATCHER_STATE_TERMINATING) {
        self->state = _DISPATCHER_STATE_TERMINATING;
        isAwaitable = true;

        if ((flags & KDISPATCH_TERMINATE_CANCEL_ALL) == KDISPATCH_TERMINATE_CANCEL_ALL) {
            deque_for_each(&self->workers, deque_node_t, it,
                kdispatch_worker_t cwp = (kdispatch_worker_t)it;

                _kdispatch_worker_drain(cwp);
            )
        }
        // Timers are drained no matter what
        _kdispatch_drain_timers(self);


        // Wake up all workers to inform them about the state change
        _kdispatch_wakeup_all_workers(self);
    }
    mtx_unlock(&self->mutex);


    if (isAwaitable && (flags & KDISPATCH_TERMINATE_AWAIT_ALL) == KDISPATCH_TERMINATE_AWAIT_ALL) {
        kdispatch_await_termination(self);
    }
}

errno_t kdispatch_await_termination(kdispatch_t _Nonnull self)
{
    decl_try_err();

    mtx_lock(&self->mutex);
    switch (self->state) {
        case _DISPATCHER_STATE_ACTIVE:
        case _DISPATCHER_STATE_SUSPENDING:
        case _DISPATCHER_STATE_SUSPENDED:
            err = ESRCH;
            break;

        case _DISPATCHER_STATE_TERMINATING:
            while (self->worker_count > 0) {
                cnd_wait(&self->cond, &self->mutex);
            }
            self->state = _DISPATCHER_STATE_TERMINATED;
            break;

        case _DISPATCHER_STATE_TERMINATED:
            break;

        default:
            abort();
    }
    mtx_unlock(&self->mutex);
    
    return err;
}
