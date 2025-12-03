//
//  dispatch_timer.c
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


static dispatch_timer_t _Nullable _dispatch_acquire_cached_timer(dispatch_t _Nonnull _Locked self)
{
    dispatch_timer_t timer;

    if (self->timer_cache.first) {
        timer = (dispatch_timer_t)SList_RemoveFirst(&self->timer_cache);
        self->timer_cache_count--;
    }
    else {
        timer = malloc(sizeof(struct dispatch_timer));
    }

    return timer;
}

static void _dispatch_cache_timer(dispatch_t _Nonnull _Locked self, dispatch_timer_t _Nonnull timer)
{
    timer->timer_qe = SLISTNODE_INIT;
    timer->item = NULL;

    if (self->timer_cache_count < _DISPATCH_MAX_TIMER_CACHE_COUNT) {
        SList_InsertBeforeFirst(&self->timer_cache, &timer->timer_qe);
        self->timer_cache_count++;
    }
    else {
        free(timer);
    }
}

void _dispatch_retire_timer(dispatch_t _Nonnull _Locked self, dispatch_timer_t _Nonnull timer)
{
    _dispatch_retire_item(self, timer->item);
    _dispatch_cache_timer(self, timer);
}

void _dispatch_drain_timers(dispatch_t _Nonnull _Locked self)
{
    while (!SList_IsEmpty(&self->timers)) {
        dispatch_timer_t ctp = (dispatch_timer_t)SList_RemoveFirst(&self->timers);

        _dispatch_retire_timer(self, ctp);
    }
}

// Removes 'item' from the timer queue and retires it.
void _dispatch_withdraw_timer_for_item(dispatch_t _Nonnull self, int flags, dispatch_item_t _Nonnull item)
{
    dispatch_timer_t ptp = NULL;
    dispatch_timer_t ctp = (dispatch_timer_t)self->timers.first;

    while (ctp) {
        dispatch_timer_t ntp = (dispatch_timer_t)ctp->timer_qe.next;

        if (ctp->item == item) {
            SList_Remove(&self->timers, &ptp->timer_qe, &ctp->timer_qe);
            _dispatch_retire_timer(self, ctp);
            break;
        }

        ptp = ctp;
        ctp = ntp;
    }
}

dispatch_timer_t _Nullable _dispatch_find_timer(dispatch_t _Nonnull self, dispatch_item_func_t _Nonnull func, void* _Nullable arg)
{
    SList_ForEach(&self->timers, SListNode, {
        dispatch_timer_t ctp = (dispatch_timer_t)pCurNode;

        if (_dispatch_item_has_func(ctp->item, func, arg)) {
             return ctp;
        }
    });

    return NULL;
}

static int _dispatch_arm_timer(dispatch_t _Nonnull _Locked self, dispatch_timer_t _Nonnull timer)
{
    dispatch_timer_t ptp = NULL;
    dispatch_timer_t ctp = (dispatch_timer_t)self->timers.first;
    

    // Make sure that we got at least one worker
    if (self->worker_count == 0) {
        if (_dispatch_acquire_worker(self) != 0) {
            return -1;
        }
    }


    timer->timer_qe = SLISTNODE_INIT;
    timer->item->state = DISPATCH_STATE_SCHEDULED;
    timer->item->flags &= ~_DISPATCH_ITEM_FLAG_CANCELLED;


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
    _dispatch_wakeup_all_workers(self);

    return 0;
}

int _dispatch_rearm_timer(dispatch_t _Nonnull _Locked self, dispatch_timer_t _Nonnull timer)
{
    // Repeating timer: rearm it with the next fire date that's in
    // the future (the next fire date we haven't already missed).
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    do  {
        timespec_add(&timer->deadline, &timer->interval, &timer->deadline);
    } while (timespec_le(&timer->deadline, &now) && timespec_gt(&timer->interval, &TIMESPEC_ZERO));
    
    return _dispatch_arm_timer(self, timer);
}

static void _calc_timer_absolute_deadline(dispatch_timer_t _Nonnull timer, int flags)
{
    if ((flags & DISPATCH_SUBMIT_ABSTIME) == 0) {
        struct timespec now;

        clock_gettime(CLOCK_MONOTONIC, &now);
        timespec_add(&now, &timer->deadline, &timer->deadline);
    }
}


////////////////////////////////////////////////////////////////////////////////
// MARK: API

int dispatch_item_after(dispatch_t _Nonnull self, int flags, const struct timespec* _Nonnull wtp, dispatch_item_t _Nonnull item)
{
    int r = -1;

    if (!timespec_isvalid(wtp)) {
        errno = EINVAL;
        return -1;
    }

    mtx_lock(&self->mutex);
    if (_dispatch_isactive(self)) {
        dispatch_timer_t timer = _dispatch_acquire_cached_timer(self);
    
        if (timer) {
            item->type = _DISPATCH_TYPE_USER_TIMER;
            item->flags = 0;
            timer->item = item;
            timer->deadline = *wtp;
            timer->interval = TIMESPEC_INF;
            _calc_timer_absolute_deadline(timer, flags);

            r = _dispatch_arm_timer(self, timer);
            if (r == -1) {
                _dispatch_cache_timer(self, timer);
            }
        }
    }
    mtx_unlock(&self->mutex);

    return r;
}

int dispatch_item_repeating(dispatch_t _Nonnull self, int flags, const struct timespec* _Nonnull wtp, const struct timespec* _Nonnull itp, dispatch_item_t _Nonnull item)
{
    int r = -1;

    if (!timespec_isvalid(wtp) || (itp && !timespec_isvalid(itp))) {
        errno = EINVAL;
        return -1;
    }

    mtx_lock(&self->mutex);
    if (_dispatch_isactive(self)) {
        dispatch_timer_t timer = _dispatch_acquire_cached_timer(self);

        if (timer) {
            item->type = _DISPATCH_TYPE_USER_TIMER;
            item->flags = _DISPATCH_ITEM_FLAG_REPEATING;
            timer->item = (dispatch_item_t)item;
            timer->deadline = *wtp;
            timer->interval = *itp;
            _calc_timer_absolute_deadline(timer, flags);

            r = _dispatch_arm_timer(self, timer);
            if (r == -1) {
                _dispatch_cache_timer(self, timer);
            }
        }
    }
    mtx_unlock(&self->mutex);

    return r;
}

int dispatch_after(dispatch_t _Nonnull self, int flags, const struct timespec* _Nonnull wtp, dispatch_async_func_t _Nonnull func, void* _Nullable arg)
{
    int r = -1;

    if (!timespec_isvalid(wtp)) {
        errno = EINVAL;
        return -1;
    }

    mtx_lock(&self->mutex);
    if (_dispatch_isactive(self)) {
        dispatch_conv_item_t item = (dispatch_conv_item_t)_dispatch_acquire_cached_conv_item(self, _async_adapter_func);
        dispatch_timer_t timer = _dispatch_acquire_cached_timer(self);
    
        if (item && timer) {
            item->super.type = _DISPATCH_TYPE_CONV_TIMER;
            item->super.flags = _DISPATCH_ITEM_FLAG_CACHEABLE;
            item->func = (int (*)(void*))func;
            item->arg = arg;
            timer->item = (dispatch_item_t)item;
            timer->deadline = *wtp;
            timer->interval = TIMESPEC_INF;
            _calc_timer_absolute_deadline(timer, flags);

            r = _dispatch_arm_timer(self, timer);
        }

        if (r == -1) {
            if (timer) _dispatch_cache_timer(self, timer);
            if (item) _dispatch_cache_item(self, (dispatch_item_t)item);
        }
    }
    mtx_unlock(&self->mutex);

    return r;
}

int dispatch_repeating(dispatch_t _Nonnull self, int flags, const struct timespec* _Nonnull wtp, const struct timespec* _Nonnull itp, dispatch_async_func_t _Nonnull func, void* _Nullable arg)
{
    int r = -1;

    if (!timespec_isvalid(wtp) || (itp && !timespec_isvalid(itp))) {
        errno = EINVAL;
        return -1;
    }

    mtx_lock(&self->mutex);
    if (_dispatch_isactive(self)) {
        dispatch_conv_item_t item = (dispatch_conv_item_t)_dispatch_acquire_cached_conv_item(self, _async_adapter_func);
        dispatch_timer_t timer = _dispatch_acquire_cached_timer(self);

        if (item && timer) {
            item->super.type = _DISPATCH_TYPE_CONV_TIMER;
            item->super.flags = _DISPATCH_ITEM_FLAG_CACHEABLE | _DISPATCH_ITEM_FLAG_REPEATING;
            item->func = (int (*)(void*))func;
            item->arg = arg;
            timer->item = (dispatch_item_t)item;
            timer->deadline = *wtp;
            timer->interval = *itp;
            _calc_timer_absolute_deadline(timer, flags);

            r = _dispatch_arm_timer(self, timer);
        }

        if (r == -1) {
            if (timer) _dispatch_cache_timer(self, timer);
            if (item) _dispatch_cache_item(self, (dispatch_item_t)item);
        }
    }
    mtx_unlock(&self->mutex);

    return r;
}
