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
#include <time.h>

static int _dispatch_acquire_worker(dispatch_t _Nonnull _Locked self);


dispatch_t _Nullable dispatch_create(const dispatch_attr_t* _Nonnull attr)
{
    dispatch_t self = NULL;
    
    if (attr->maxConcurrency < 1 || attr->maxConcurrency > INT8_MAX || attr->minConcurrency > attr->maxConcurrency) {
        errno = EINVAL;
        return NULL;
    }
    if (attr->qos < 0 || attr->qos >= DISPATCH_QOS_COUNT) {
        errno = EINVAL;
        return NULL;
    }
    if (attr->priority < DISPATCH_PRI_LOWEST || attr->priority >= DISPATCH_PRI_HIGHEST) {
        errno = EINVAL;
        return NULL;
    }


    self = calloc(1, sizeof(struct dispatch));
    if (self == NULL) {
        return NULL;
    }

    if (mutex_init(&self->mutex) != 0) {
        free(self);
        return NULL;
    }

    self->attr = *attr;
    self->attr.cb = &self->cb;
    if (attr->cb) {
        self->cb = *(attr->cb);
    }

    self->workers = LIST_INIT;
    self->worker_count = 0;
    self->item_cache = SLIST_INIT;
    self->item_cache_count = 0;
    self->zombies = SLIST_INIT;
    self->timers = SLIST_INIT;
    self->timer_cache = SLIST_INIT;
    self->timer_cache_count = 0;
    self->state = _DISPATCHER_STATE_ACTIVE;

    if (cond_init(&self->cond) != 0) {
        dispatch_destroy(self);
        return NULL;
    }

    for (size_t i = 0; i < attr->minConcurrency; i++) {
        if (_dispatch_acquire_worker(self) != 0) {
            dispatch_destroy(self);
            return NULL;
        }
    }

    return self;
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

static int _dispatch_acquire_worker(dispatch_t _Nonnull _Locked self)
{
    dispatch_worker_t worker = _dispatch_worker_create(&self->attr, self);

    if (worker) {
        List_InsertAfterLast(&self->workers, &worker->worker_qe);
        self->worker_count++;

        return 0;
    }

    return -1;
}

_Noreturn _dispatch_relinquish_worker(dispatch_t _Nonnull _Locked self, dispatch_worker_t _Nonnull worker)
{
    List_Remove(&self->workers, &worker->worker_qe);
    self->worker_count--;

    _dispatch_worker_destroy(worker);

    cond_broadcast(&self->cond);
    mutex_unlock(&self->mutex);

    vcpu_relinquish_self();
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


    // Acquire a worker if we don't have one
    // XXX improve this and take advantage of max concurrency
    if (self->worker_count == 0) {
        if (_dispatch_acquire_worker(self) != 0) {
            return -1;
        }
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
    }
    item->state = DISPATCH_STATE_PENDING;
    item->qe = SLISTNODE_INIT;


    // Enqueue the work item at the worker that we found and notify it
    _dispatch_worker_submit(best_wp, item);

    return 0;
}

void _dispatch_retire_item(dispatch_t _Nonnull _Locked self, dispatch_item_t _Nonnull item)
{
    if ((item->flags & DISPATCH_ITEM_JOINABLE) != 0) {
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

static int _dispatch_join(dispatch_t _Nonnull _Locked self, dispatch_item_t _Nonnull item)
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


////////////////////////////////////////////////////////////////////////////////
// MARK: Item Cache

static dispatch_cacheable_item_t _Nullable _dispatch_acquire_cached_item(dispatch_t _Nonnull _Locked self, size_t nbytes, dispatch_item_func_t itemFunc, uint16_t flags)
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
        ip->super.itemFunc = itemFunc;
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
// MARK: Timers

static dispatch_timer_t _Nullable _dispatch_acquire_cached_timer(dispatch_t _Nonnull _Locked self)
{
    dispatch_timer_t timer;

    if (self->timer_cache.first) {
        timer = (dispatch_timer_t)SList_RemoveFirst(&self->timer_cache);
    }
    else {
        timer = malloc(sizeof(struct dispatch_timer));
    }

    return timer;
}

void _dispatch_cache_timer(dispatch_t _Nonnull _Locked self, dispatch_timer_t _Nonnull timer)
{
    if (self->timer_cache_count >= _DISPATCH_MAX_TIMER_CACHE_COUNT) {
        free(timer);
        return;
    }

    SList_InsertBeforeFirst(&self->timer_cache, &timer->timer_qe);
    self->timer_cache_count++;
}

static void _dispatch_retire_timer(dispatch_t _Nonnull _Locked self, dispatch_timer_t _Nonnull timer)
{
    _dispatch_retire_item(self, timer->item);
    timer->item = NULL;
    _dispatch_cache_timer(self, timer);
}

static void _dispatch_drain_timers(dispatch_t _Nonnull _Locked self)
{
    SList_ForEach(&self->timers, ListNode, {
        dispatch_timer_t ctp = (dispatch_timer_t)pCurNode;

        _dispatch_retire_timer(self, ctp);
    });
}

static int _dispatch_arm_timer(dispatch_t _Nonnull _Locked self, dispatch_timer_t _Nonnull timer)
{
    dispatch_timer_t ptp = NULL;
    dispatch_timer_t ctp = (dispatch_timer_t)self->timers.first;
    

    // Acquire a worker if we don't have one
    // XXX improve this and take advantage of max concurrency
    if (self->worker_count == 0) {
        if (_dispatch_acquire_worker(self) != 0) {
            return -1;
        }
    }


    timer->item->state = DISPATCH_STATE_PENDING;
    timer->item->qe = SLISTNODE_INIT;


    // Put the timer on the timer queue. The timer queue is sorted by absolute
    // timer fire time (ascending). Timers with the same fire time are added in
    // FIFO order.
    while (ctp) {
        if (timespec_gt(&ctp->deadline, &timer->deadline)) {
            break;
        }
        
        ptp = ctp;
        ctp = (dispatch_timer_t)ctp->timer_qe.next;
    }
    
    SList_InsertAfter(&self->timers, &timer->timer_qe, &ptp->timer_qe);


    // Notify all workers.
    // XXX improve this. Not ideal that we might cause a wakeup storm where we
    // XXX wake up all workers though only one is needed to execute the timer.
    List_ForEach(&self->workers, ListNode, {
        dispatch_worker_t cwp = (dispatch_worker_t)pCurNode;

        _dispatch_worker_wakeup(cwp);
    });

    return 0;
}

static int _dispatch_rearm_timer(dispatch_t _Nonnull _Locked self, dispatch_timer_t _Nonnull timer)
{
    // Repeating timer: rearm it with the next fire date that's in
    // the future (the next fire date we haven't already missed).
    struct timespec now;
    
    clock_gettime(CLOCK_MONOTONIC, &now);
    
    do  {
        timespec_add(&timer->deadline, &timer->interval, &timer->deadline);
    } while (timespec_lt(&timer->deadline, &now));
    
    return _dispatch_arm_timer(self, timer);
}

static int _dispatch_timer(dispatch_t _Nonnull _Locked self, dispatch_item_t _Nonnull item, int flags, const struct timespec* _Nonnull deadline, const struct timespec* _Nullable interval)
{
    dispatch_timer_t timer;
    
    if (!timespec_isvalid(deadline) || (interval && !timespec_isvalid(interval))) {
        errno = EINVAL;
        return -1;
    }
    if (item->state != DISPATCH_STATE_IDLE) {
        errno = EBUSY;
        return -1;
    }


    // Allocate a timer and set it up to execute 'item' at or after 'deadline'
    timer = _dispatch_acquire_cached_timer(self);
    if (timer == NULL) {
        return -1;
    }
    timer->timer_qe = SLISTNODE_INIT;
    timer->item = item;
    timer->flags = 0;

    if ((flags & TIMER_ABSTIME) == TIMER_ABSTIME) {
        timer->deadline = *deadline;
    }
    else {
        struct timespec now;

        clock_gettime(CLOCK_MONOTONIC, &now);
        timespec_add(&now, deadline, &timer->deadline);
    }
    if (interval && timespec_lt(interval, &TIMESPEC_INF)) {
        timer->interval = *interval;
        timer->flags |= _DISPATCH_TIMER_REPEATING;
    }


    // Arm the timer (add it to our timer queue)
    const int r = _dispatch_arm_timer(self, timer);
    if (r != 0) {
        _dispatch_cache_timer(self, timer);
    }

    return r;
}


////////////////////////////////////////////////////////////////////////////////
// MARK: API

int dispatch_submit(dispatch_t _Nonnull self, dispatch_item_t _Nonnull item)
{
    int r;

    mutex_lock(&self->mutex);
    if (self->state == _DISPATCHER_STATE_ACTIVE) {
        item->flags &= _DISPATCH_ITEM_PUBLIC_MASK;
        r = _dispatch_submit(self, item);
    }
    else {
        errno = ETERMINATED;
        r = -1;
    }
    mutex_unlock(&self->mutex);
    return r;
}

int dispatch_join(dispatch_t _Nonnull self, dispatch_item_t _Nonnull item)
{
    mutex_lock(&self->mutex);
    const int r = _dispatch_join(self, item);
    mutex_unlock(&self->mutex);
    return r;
}


static void _async_adapter_func(dispatch_item_t _Nonnull item)
{
    dispatch_async_item_t async_item = (dispatch_async_item_t)item;

    async_item->func(async_item->context);
}

int dispatch_async(dispatch_t _Nonnull self, dispatch_async_func_t _Nonnull func, void* _Nullable context)
{
    int r = -1;

    mutex_lock(&self->mutex);
    if (self->state == _DISPATCHER_STATE_ACTIVE) {
       dispatch_cacheable_item_t item = _dispatch_acquire_cached_item(self, sizeof(struct dispatch_async_item), _async_adapter_func, _DISPATCH_ITEM_CACHEABLE);
    
        if (item) {
            ((dispatch_async_item_t)item)->func = func;
            ((dispatch_async_item_t)item)->context = context;
            r = _dispatch_submit(self, (dispatch_item_t)item);
            if (r != 0) {
                _dispatch_cache_item(self, item);
            }
        }
    }
    else {
        errno = ETERMINATED;
    }
    mutex_unlock(&self->mutex);

    return r;
}


static void _sync_adapter_func(dispatch_item_t _Nonnull item)
{
    dispatch_sync_item_t sync_item = (dispatch_sync_item_t)item;

    sync_item->result = sync_item->func(sync_item->context);
}

int dispatch_sync(dispatch_t _Nonnull self, dispatch_sync_func_t _Nonnull func, void* _Nullable context)
{
    int r = -1;

    mutex_lock(&self->mutex);
    if (self->state == _DISPATCHER_STATE_ACTIVE) {
        dispatch_cacheable_item_t item = _dispatch_acquire_cached_item(self, sizeof(struct dispatch_sync_item), _sync_adapter_func, _DISPATCH_ITEM_CACHEABLE | DISPATCH_ITEM_JOINABLE);
    
        if (item) {
            ((dispatch_sync_item_t)item)->func = func;
            ((dispatch_sync_item_t)item)->context = context;
            ((dispatch_sync_item_t)item)->result = 0;
            if (_dispatch_submit(self, (dispatch_item_t)item) == 0) {
                r = _dispatch_join(self, (dispatch_item_t)item);
                if (r == 0) {
                    r = ((dispatch_sync_item_t)item)->result;
                }
            }
            _dispatch_cache_item(self, item);
        }
    }
    else {
        errno = ETERMINATED;
    }
    mutex_unlock(&self->mutex);

    return r;
}


int dispatch_timer(dispatch_t _Nonnull self, dispatch_item_t _Nonnull item, int flags, const struct timespec* _Nonnull deadline, const struct timespec* _Nullable interval)
{
    int r;

    mutex_lock(&self->mutex);
    if (self->state == _DISPATCHER_STATE_ACTIVE) {
        item->flags &= _DISPATCH_ITEM_PUBLIC_MASK;
        r = _dispatch_timer(self, item, flags, deadline, interval);
    }
    else {
        errno = ETERMINATED;
        r = -1;
    }
    mutex_unlock(&self->mutex);
    return r;
}

void dispatch_cancel_timer(dispatch_t _Nonnull self, dispatch_item_t _Nonnull item)
{
    dispatch_timer_t ptp = NULL;
    dispatch_timer_t ctp;

    mutex_lock(&self->mutex);
    ctp = (dispatch_timer_t)self->timers.first;
    while (ctp) {
        dispatch_timer_t ntp = (dispatch_timer_t)ctp->timer_qe.next;

        if (ctp->item == item) {
            SList_Remove(&self->timers, &ptp->timer_qe, &ctp->timer_qe);
            _dispatch_retire_timer(self, ctp);
        }

        ptp = ctp;
        ctp = ntp;
    }
    mutex_unlock(&self->mutex);
}

int dispatch_after(dispatch_t _Nonnull self, int flags, const struct timespec* _Nonnull wtp, dispatch_async_func_t _Nonnull func, void* _Nullable context)
{
    int r = -1;

    mutex_lock(&self->mutex);
    if (self->state == _DISPATCHER_STATE_ACTIVE) {
       dispatch_cacheable_item_t item = _dispatch_acquire_cached_item(self, sizeof(struct dispatch_async_item), _async_adapter_func, _DISPATCH_ITEM_CACHEABLE);
    
        if (item) {
            ((dispatch_async_item_t)item)->func = func;
            ((dispatch_async_item_t)item)->context = context;
            r = _dispatch_timer(self, (dispatch_item_t)item, flags, wtp, NULL);
            if (r != 0) {
                _dispatch_cache_item(self, item);
            }
        }
    }
    else {
        errno = ETERMINATED;
    }
    mutex_unlock(&self->mutex);

    return r;
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
