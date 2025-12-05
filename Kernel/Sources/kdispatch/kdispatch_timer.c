//
//  dispatch_timer.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/10/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "kdispatch_priv.h"


static kdispatch_timer_t _Nullable _kdispatch_acquire_cached_timer(kdispatch_t _Nonnull _Locked self)
{
    kdispatch_timer_t timer;

    if (self->timer_cache.first) {
        timer = (kdispatch_timer_t)SList_RemoveFirst(&self->timer_cache);
        self->timer_cache_count--;
    }
    else {
        kalloc(sizeof(struct kdispatch_timer), (void**)&timer);
    }

    return timer;
}

static void _kdispatch_cache_timer(kdispatch_t _Nonnull _Locked self, kdispatch_timer_t _Nonnull timer)
{
    timer->timer_qe = SLISTNODE_INIT;
    timer->item = NULL;

    if (self->timer_cache_count < _KDISPATCH_MAX_TIMER_CACHE_COUNT) {
        SList_InsertBeforeFirst(&self->timer_cache, &timer->timer_qe);
        self->timer_cache_count++;
    }
    else {
        kfree(timer);
    }
}

void _kdispatch_retire_timer(kdispatch_t _Nonnull _Locked self, kdispatch_timer_t _Nonnull timer)
{
    _kdispatch_retire_item(self, timer->item);
    _kdispatch_cache_timer(self, timer);
}

void _kdispatch_drain_timers(kdispatch_t _Nonnull _Locked self)
{
    while (!SList_IsEmpty(&self->timers)) {
        kdispatch_timer_t ctp = (kdispatch_timer_t)SList_RemoveFirst(&self->timers);

        _kdispatch_retire_timer(self, ctp);
    }
}

// Removes 'item' from the timer queue and retires it.
void _kdispatch_withdraw_timer_for_item(kdispatch_t _Nonnull self, kdispatch_item_t _Nonnull item)
{
    kdispatch_timer_t ptp = NULL;
    kdispatch_timer_t ctp = (kdispatch_timer_t)self->timers.first;

    while (ctp) {
        kdispatch_timer_t ntp = (kdispatch_timer_t)ctp->timer_qe.next;

        if (ctp->item == item) {
            SList_Remove(&self->timers, &ptp->timer_qe, &ctp->timer_qe);
            _kdispatch_retire_timer(self, ctp);
            break;
        }

        ptp = ctp;
        ctp = ntp;
    }
}

kdispatch_timer_t _Nullable _kdispatch_find_timer(kdispatch_t _Nonnull self, kdispatch_item_func_t _Nonnull func, void* _Nullable arg)
{
    SList_ForEach(&self->timers, SListNode, {
        kdispatch_timer_t ctp = (kdispatch_timer_t)pCurNode;

        if (_kdispatch_item_has_func(ctp->item, func, arg)) {
             return ctp;
        }
    });

    return NULL;
}

static void _kdispatch_queue_timer(kdispatch_t _Nonnull self, kdispatch_timer_t _Nonnull timer)
{
    kdispatch_timer_t ptp = NULL;
    kdispatch_timer_t ctp = (kdispatch_timer_t)self->timers.first;

    // Put the timer on the timer queue. The timer queue is sorted by absolute
    // timer fire time (ascending). Timers with the same fire time are added in
    // FIFO order.
    while (ctp) {
        if (timespec_gt(&ctp->deadline, &timer->deadline)) {
            break;
        }
        
        ptp = ctp;
        ctp = (kdispatch_timer_t)ctp->timer_qe.next;
    }
    
    SList_InsertAfter(&self->timers, &timer->timer_qe, &ptp->timer_qe);
}

static errno_t _kdispatch_arm_timer(kdispatch_t _Nonnull _Locked self, int flags, const struct timespec* _Nonnull wtp, const struct timespec* _Nonnull itp, kdispatch_item_t _Nonnull item)
{
    decl_try_err();

    // Make sure that we got at least one worker
    if (self->worker_count == 0) {
        err = _kdispatch_acquire_worker(self);
        if (err != EOK) {
            return err;
        }
    }


    kdispatch_timer_t timer = _kdispatch_acquire_cached_timer(self); 
    if (timer == NULL) {
        return ENOMEM;
    }


    item->state = KDISPATCH_STATE_SCHEDULED;
    item->flags &= ~_KDISPATCH_ITEM_FLAG_CANCELLED;

    timer->timer_qe = SLISTNODE_INIT;
    timer->item = item;
    timer->deadline = *wtp;
    timer->interval = *itp;

    if ((flags & KDISPATCH_SUBMIT_ABSTIME) == 0) {
        struct timespec now;

        clock_gettime(g_mono_clock, &now);
        timespec_add(&now, &timer->deadline, &timer->deadline);
    }


    _kdispatch_queue_timer(self, timer);


    // Notify all workers.
    // XXX improve this. Not ideal that we might cause a wakeup storm where we
    // XXX wake up all workers though only one is needed to execute the timer.
    _kdispatch_wakeup_all_workers(self);

    return EOK;
}

void _kdispatch_rearm_timer(kdispatch_t _Nonnull _Locked self, kdispatch_timer_t _Nonnull timer)
{
    struct timespec now;

    // Repeating timer: rearm it with the next fire date that's in
    // the future (the next fire date we haven't already missed).
    clock_gettime(g_mono_clock, &now);
    do  {
        timespec_add(&timer->deadline, &timer->interval, &timer->deadline);
    } while (timespec_le(&timer->deadline, &now) && timespec_gt(&timer->interval, &TIMESPEC_ZERO));


    timer->timer_qe = SLISTNODE_INIT;
    timer->item->state = KDISPATCH_STATE_SCHEDULED;
    timer->item->flags &= ~_KDISPATCH_ITEM_FLAG_CANCELLED;


    _kdispatch_queue_timer(self, timer);

    // No need to wake up workers because this is getting called from a worker
    // and thus at least one worker is clearly alive.
}


////////////////////////////////////////////////////////////////////////////////
// MARK: API

errno_t kdispatch_item_after(kdispatch_t _Nonnull self, int flags, const struct timespec* _Nonnull wtp, kdispatch_item_t _Nonnull item)
{
    decl_try_err();

    if (!timespec_isvalid(wtp)) {
        return EINVAL;
    }

    mtx_lock(&self->mutex);
    if (self->state >= _DISPATCHER_STATE_TERMINATING) {
        throw(ETERMINATED);
    }
    if (item->state == KDISPATCH_STATE_SCHEDULED || item->state == KDISPATCH_STATE_EXECUTING) {
        throw(EBUSY);
    }

    item->type = _KDISPATCH_TYPE_USER_TIMER;
    item->flags = 0;

    err = _kdispatch_arm_timer(self, flags, wtp, &TIMESPEC_INF, item);

catch:
    mtx_unlock(&self->mutex);

    return err;
}

errno_t kdispatch_item_repeating(kdispatch_t _Nonnull self, int flags, const struct timespec* _Nonnull wtp, const struct timespec* _Nonnull itp, kdispatch_item_t _Nonnull item)
{
    decl_try_err();

    if (!timespec_isvalid(wtp) || (itp && !timespec_isvalid(itp))) {
        return EINVAL;
    }

    mtx_lock(&self->mutex);
    if (self->state >= _DISPATCHER_STATE_TERMINATING) {
        throw(ETERMINATED);
    }
    if (item->state == KDISPATCH_STATE_SCHEDULED || item->state == KDISPATCH_STATE_EXECUTING) {
        throw(EBUSY);
    }

    item->type = _KDISPATCH_TYPE_USER_TIMER;
    item->flags = _KDISPATCH_ITEM_FLAG_REPEATING;

    err = _kdispatch_arm_timer(self, flags, wtp, itp, item);

catch:
    mtx_unlock(&self->mutex);

    return err;
}

errno_t kdispatch_after(kdispatch_t _Nonnull self, int flags, const struct timespec* _Nonnull wtp, kdispatch_async_func_t _Nonnull func, void* _Nullable arg)
{
    decl_try_err();

    if (!timespec_isvalid(wtp)) {
        return EINVAL;
    }

    mtx_lock(&self->mutex);
    if (self->state < _DISPATCHER_STATE_TERMINATING) {
        kdispatch_conv_item_t item = (kdispatch_conv_item_t)_kdispatch_acquire_cached_conv_item(self, _async_adapter_func);
    
        if (item) {
            item->super.type = _KDISPATCH_TYPE_CONV_TIMER;
            item->super.flags = _KDISPATCH_ITEM_FLAG_CACHEABLE;
            item->func = (int (*)(void*))func;
            item->arg = arg;

            err = _kdispatch_arm_timer(self, flags, wtp, &TIMESPEC_INF, (kdispatch_item_t)item);
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

errno_t kdispatch_repeating(kdispatch_t _Nonnull self, int flags, const struct timespec* _Nonnull wtp, const struct timespec* _Nonnull itp, kdispatch_async_func_t _Nonnull func, void* _Nullable arg)
{
    decl_try_err();

    if (!timespec_isvalid(wtp) || (itp && !timespec_isvalid(itp))) {
        return EINVAL;
    }

    mtx_lock(&self->mutex);
    if (self->state < _DISPATCHER_STATE_TERMINATING) {
        kdispatch_conv_item_t item = (kdispatch_conv_item_t)_kdispatch_acquire_cached_conv_item(self, _async_adapter_func);

        if (item) {
            item->super.type = _KDISPATCH_TYPE_CONV_TIMER;
            item->super.flags = _KDISPATCH_ITEM_FLAG_CACHEABLE | _KDISPATCH_ITEM_FLAG_REPEATING;
            item->func = (int (*)(void*))func;
            item->arg = arg;

            err = _kdispatch_arm_timer(self, flags, wtp, itp, (kdispatch_item_t)item);
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
