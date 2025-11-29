//
//  dispatch.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/10/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "dispatch_priv.h"
#include <kern/string.h>
#include <process/Process.h>


static errno_t _dispatch_init(dispatch_t _Nonnull self, const dispatch_attr_t* _Nonnull attr)
{
    decl_try_err();

    if (attr->maxConcurrency < 1 || attr->maxConcurrency > INT8_MAX || attr->minConcurrency > attr->maxConcurrency) {
        return EINVAL;
    }
    if (attr->qos < DISPATCH_QOS_BACKGROUND || attr->qos > DISPATCH_QOS_REALTIME) {
        return EINVAL;
    }
    if (attr->priority < DISPATCH_PRI_LOWEST || attr->priority >= DISPATCH_PRI_HIGHEST) {
        return EINVAL;
    }


    memset(self, 0, sizeof(struct dispatch));

    mtx_init(&self->mutex);

    self->attr = *attr;
    self->groupid = new_vcpu_groupid();
    self->state = _DISPATCHER_STATE_ACTIVE;
    self->item_cache[_DISPATCH_ITEM_CACHE_IDX(_DISPATCH_TYPE_CONV_ITEM)].maxCount = _DISPATCH_MAX_CONV_ITEM_CACHE_COUNT;
    self->item_cache[_DISPATCH_ITEM_CACHE_IDX(_DISPATCH_TYPE_CONV_TIMER)].maxCount = _DISPATCH_MAX_CONV_TIMER_CACHE_COUNT;

    cnd_init(&self->cond);

    for (size_t i = 0; i < attr->minConcurrency; i++) {
        err = _dispatch_acquire_worker(self);
        if (err != EOK) {
            return err;
        }
    }

    if (attr->name && attr->name[0] != '\0') {
        String_CopyUpTo(self->name, attr->name, DISPATCH_MAX_NAME_LENGTH);
    }

    return EOK;
}

errno_t dispatch_create(const dispatch_attr_t* _Nonnull attr, dispatch_t _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    dispatch_t self = NULL;
    
    err = kalloc_cleared(sizeof(struct dispatch), (void**)&self);
    if (err == EOK) {
        err = _dispatch_init(self, attr);
        if (err != EOK) {
            kfree(self);
            self = NULL;
        }
    }

    *pOutSelf = self;
    return err;
}

errno_t dispatch_destroy(dispatch_t _Nullable self)
{
    if (self) {
        if (self->state < _DISPATCHER_STATE_TERMINATED || !SList_IsEmpty(&self->zombie_items)) {
            return EBUSY;
        }


        SList_ForEach(&self->timer_cache, SListNode, {
            kfree(pCurNode);
        });
        self->timer_cache = SLIST_INIT;


        for (int i = 0; i < _DISPATCH_ITEM_CACHE_COUNT; i++) {
            SList_ForEach(&self->item_cache[i].items, SListNode, {
                kfree(pCurNode);
            });
            self->item_cache[i].items = SLIST_INIT;
            self->item_cache[i].count = 0;
        }


        if (self->sigtraps) {
            kfree(self->sigtraps);
            self->sigtraps = NULL;
        }

        self->workers = LIST_INIT;
        self->zombie_items = SLIST_INIT;
        self->timers = SLIST_INIT;

        cnd_deinit(&self->cond);
        mtx_deinit(&self->mutex);

        kfree(self);
    }

    return 0;
}


////////////////////////////////////////////////////////////////////////////////
// MARK: Workers

errno_t _dispatch_acquire_worker(dispatch_t _Nonnull _Locked self)
{
    decl_try_err();
    dispatch_worker_t worker;
    
    err = _dispatch_worker_create(self, &worker);
    if (err != EOK) {
        return err;
    }

    List_InsertAfterLast(&self->workers, &worker->worker_qe);
    self->worker_count++;

    return EOK;
}

_Noreturn _dispatch_relinquish_worker(dispatch_t _Nonnull _Locked self, dispatch_worker_t _Nonnull worker)
{
    List_Remove(&self->workers, &worker->worker_qe);
    self->worker_count--;

    _dispatch_worker_destroy(worker);

    cnd_broadcast(&self->cond);
    mtx_unlock(&self->mutex);

    Process_RelinquishVirtualProcessor(gKernelProcess, vcpu_current());
    /* NO RETURN */
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

static errno_t _dispatch_submit(dispatch_t _Nonnull _Locked self, dispatch_item_t _Nonnull item)
{
    decl_try_err();
    dispatch_worker_t best_wp = NULL;
    size_t best_wc = SIZE_MAX;

    // Allow the submission of an Idle, Finished or Cancelled state item
    if (item->state == DISPATCH_STATE_SCHEDULED || item->state == DISPATCH_STATE_EXECUTING) {
        return EBUSY;
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
    else if (self->worker_count > 0) {
        best_wp = (dispatch_worker_t)self->workers.first;
        best_wc = best_wp->work_count;
    }


    // Worker:
    // - need at least one
    // - spawn another one if the 'best' worker has too much stuff queued and
    //   we haven't reached the max number of workers yet
    if (self->worker_count == 0 || (best_wc > 4 && self->worker_count < self->attr.maxConcurrency)) {
        err = _dispatch_acquire_worker(self);

        // Don't make the submit fail outright if we can't allocate a new worker
        // and this was just about adding one more.
        if (err != EOK && self->worker_count == 0) {
            return err;
        }

        best_wp = (dispatch_worker_t)self->workers.first;
        best_wc = best_wp->work_count;
    }

    item->qe = SLISTNODE_INIT;
    item->state = DISPATCH_STATE_SCHEDULED;
    item->flags &= ~_DISPATCH_ITEM_FLAG_CANCELLED;


    // Enqueue the work item at the worker that we found and notify it
    _dispatch_worker_submit(best_wp, item, true);

    return EOK;
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

static errno_t _dispatch_await(dispatch_t _Nonnull _Locked self, dispatch_item_t _Nonnull item)
{
    decl_try_err();

    while (item->state < DISPATCH_STATE_FINISHED) {
        err = cnd_wait(&self->cond, &self->mutex);
        if (err != EOK) {
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

    return EOK;
}

void _dispatch_zombify_item(dispatch_t _Nonnull _Locked self, dispatch_item_t _Nonnull item)
{
    SList_InsertAfterLast(&self->zombie_items, &item->qe);
    cnd_broadcast(&self->cond);
}

static dispatch_item_t _Nullable _dispatch_find_item(dispatch_t _Nonnull self, dispatch_item_func_t _Nonnull func, void* _Nullable arg)
{
    List_ForEach(&self->workers, ListNode, {
        dispatch_worker_t cwp = (dispatch_worker_t)pCurNode;
        dispatch_item_t ip = _dispatch_worker_find_item(cwp, func, arg);

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
        kalloc(nbytes, (void**)&ip);
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
        kfree(item);
    }
}


////////////////////////////////////////////////////////////////////////////////
// MARK: API

errno_t dispatch_submit(dispatch_t _Nonnull self, int flags, dispatch_item_t _Nonnull item)
{
    decl_try_err();
    
    if (item->func == NULL) {
        return EINVAL;
    }

    mtx_lock(&self->mutex);
    if (self->state < _DISPATCHER_STATE_TERMINATING) {
        item->type = _DISPATCH_TYPE_USER_ITEM;
        item->flags = (uint8_t)(flags & _DISPATCH_ITEM_FLAG_AWAITABLE);
        err = _dispatch_submit(self, item);
    }
    else {
        err = ETERMINATED;
    }
    mtx_unlock(&self->mutex);
    return err;
}

errno_t dispatch_await(dispatch_t _Nonnull self, dispatch_item_t _Nonnull item)
{
    decl_try_err();

    mtx_lock(&self->mutex);
    err = _dispatch_await(self, item);
    mtx_unlock(&self->mutex);
    return err;
}


static void _async_adapter_func(dispatch_item_t _Nonnull item)
{
    dispatch_conv_item_t ip = (dispatch_conv_item_t)item;

    (void)ip->func(ip->arg);
}

errno_t dispatch_async(dispatch_t _Nonnull self, dispatch_async_func_t _Nonnull func, void* _Nullable arg)
{
    decl_try_err();

    mtx_lock(&self->mutex);
    if (self->state < _DISPATCHER_STATE_TERMINATING) {
       dispatch_conv_item_t item = (dispatch_conv_item_t)_dispatch_acquire_cached_item(self, _DISPATCH_TYPE_CONV_ITEM, _async_adapter_func);
    
        if (item) {
            item->super.flags = _DISPATCH_ITEM_FLAG_CACHEABLE;
            item->func = (int (*)(void*))func;
            item->arg = arg;
            err = _dispatch_submit(self, (dispatch_item_t)item);
            if (err != EOK) {
                _dispatch_cache_item(self, (dispatch_item_t)item);
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


static void _sync_adapter_func(dispatch_item_t _Nonnull item)
{
    dispatch_conv_item_t ip = (dispatch_conv_item_t)item;

    ip->result = ip->func(ip->arg);
}

errno_t dispatch_sync(dispatch_t _Nonnull self, dispatch_sync_func_t _Nonnull func, void* _Nullable arg)
{
    decl_try_err();

    mtx_lock(&self->mutex);
    if (self->state < _DISPATCHER_STATE_TERMINATING) {
        dispatch_conv_item_t item = (dispatch_conv_item_t)_dispatch_acquire_cached_item(self, _DISPATCH_TYPE_CONV_ITEM, _sync_adapter_func);
    
        if (item) {
            item->super.flags = _DISPATCH_ITEM_FLAG_CACHEABLE | _DISPATCH_ITEM_FLAG_AWAITABLE;
            item->func = (int (*)(void*))func;
            item->arg = arg;
            item->result = 0;
            if (_dispatch_submit(self, (dispatch_item_t)item) == 0) {
                err = _dispatch_await(self, (dispatch_item_t)item);
                if (err == EOK) {
                    err = item->result;
                }
            }
            _dispatch_cache_item(self, (dispatch_item_t)item);
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

void dispatch_cancel(dispatch_t _Nonnull self, int flags, dispatch_item_func_t _Nonnull func, void* _Nullable arg)
{
    mtx_lock(&self->mutex);
    dispatch_item_t item = (dispatch_item_t)_dispatch_find_timer(self, func, arg);

    if (item == NULL) {
        item = _dispatch_find_item(self, func, arg);
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

errno_t dispatch_setpriority(dispatch_t _Nonnull self, int priority)
{
    if (priority < DISPATCH_PRI_LOWEST || priority > DISPATCH_PRI_HIGHEST) {
        return EINVAL;
    }

    mtx_lock(&self->mutex);
    _dispatch_applyschedparams(self, self->attr.qos, priority);
    mtx_unlock(&self->mutex);
    
    return EOK;
}

int dispatch_qos(dispatch_t _Nonnull self)
{
    mtx_lock(&self->mutex);
    const int r = self->attr.qos;
    mtx_unlock(&self->mutex);
    return r;
}

errno_t dispatch_setqos(dispatch_t _Nonnull self, int qos)
{
    if (qos < DISPATCH_QOS_BACKGROUND || qos > DISPATCH_QOS_REALTIME) {
        return EINVAL;
    }

    mtx_lock(&self->mutex);
    _dispatch_applyschedparams(self, qos, self->attr.priority);
    mtx_unlock(&self->mutex);
    
    return EOK;
}

void dispatch_concurrency_info(dispatch_t _Nonnull self, dispatch_concurrency_info_t* _Nonnull info)
{
    mtx_lock(&self->mutex);
    info->minimum = self->attr.minConcurrency;
    info->maximum = self->attr.maxConcurrency;
    info->current = self->worker_count;
    mtx_unlock(&self->mutex);
}

errno_t dispatch_name(dispatch_t _Nonnull self, char* _Nonnull buf, size_t buflen)
{
    decl_try_err();

    mtx_lock(&self->mutex);
    const size_t len = String_Length(self->name);

    if (buflen == 0) {
        throw(EINVAL);
    }
    if (buflen < (len + 1)) {
        throw(ERANGE);
    }
    String_Copy(buf, self->name);

catch:
    mtx_unlock(&self->mutex);
    return err;
}


errno_t dispatch_suspend(dispatch_t _Nonnull self)
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
    }
    else {
        err = ETERMINATED;
    }

    mtx_unlock(&self->mutex);
    return err;
}

void dispatch_resume(dispatch_t _Nonnull self)
{
    mtx_lock(&self->mutex);

    if (self->state < _DISPATCHER_STATE_TERMINATING) {
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
    if (self->state < _DISPATCHER_STATE_TERMINATING) {
        self->state = _DISPATCHER_STATE_TERMINATING;
        isAwaitable = true;

        if ((flags & DISPATCH_TERMINATE_CANCEL_ALL) == DISPATCH_TERMINATE_CANCEL_ALL) {
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


    if (isAwaitable && (flags & DISPATCH_TERMINATE_AWAIT_ALL) == DISPATCH_TERMINATE_AWAIT_ALL) {
        dispatch_await_termination(self);
    }
}

errno_t dispatch_await_termination(dispatch_t _Nonnull self)
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
