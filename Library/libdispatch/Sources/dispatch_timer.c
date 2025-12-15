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


static void _dispatch_queue_timer(dispatch_t _Nonnull self, dispatch_timer_t _Nonnull timer)
{
    dispatch_timer_t ptp = NULL;
    dispatch_timer_t ctp = (dispatch_timer_t)self->timers.first;

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
    
    
    if (ptp) {
        SList_InsertAfter(&self->timers, &timer->timer_qe, &ptp->timer_qe);
    }
    else {
        SList_InsertBeforeFirst(&self->timers, &timer->timer_qe);
    }
}

static dispatch_timer_t _Nullable _dispatch_dequeue_timer_for_item(dispatch_t _Nonnull self, dispatch_item_t _Nonnull item)
{
    dispatch_timer_t ptp = NULL;
    dispatch_timer_t timer = NULL;

    SList_ForEach(&self->timers, SListNode,{
        dispatch_timer_t ctp = (dispatch_timer_t)pCurNode;

        if (ctp->item == item) {
            timer = ctp;
            break;
        }

        ptp = ctp;
    })


    if (timer) {
        if (ptp) {
            SList_Remove(&self->timers, &ptp->timer_qe, &timer->timer_qe);
        }
        else {
            SList_RemoveFirst(&self->timers);
        }
    }

    return timer;
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


void _dispatch_retire_timer(dispatch_t _Nonnull _Locked self, dispatch_timer_t _Nonnull timer)
{
    dispatch_item_t item = timer->item;

    timer->timer_qe = SLISTNODE_INIT;
    timer->item = NULL;

    if (self->timer_cache_count < _DISPATCH_MAX_TIMER_CACHE_COUNT) {
        SList_InsertBeforeFirst(&self->timer_cache, &timer->timer_qe);
        self->timer_cache_count++;
    }
    else {
        free(timer);
    }


    _dispatch_retire_item(self, item);
}

void _dispatch_drain_timers(dispatch_t _Nonnull _Locked self)
{
    while (!SList_IsEmpty(&self->timers)) {
        dispatch_timer_t ctp = (dispatch_timer_t)SList_RemoveFirst(&self->timers);

        _dispatch_retire_timer(self, ctp);
    }
}

// Removes 'item' from the timer queue and retires it.
void _dispatch_withdraw_timer_for_item(dispatch_t _Nonnull self, dispatch_item_t _Nonnull item)
{
    dispatch_timer_t timer = _dispatch_dequeue_timer_for_item(self, item);

    if (timer) {
        _dispatch_retire_timer(self, timer);
    }
}


static int _dispatch_arm_timer(dispatch_t _Nonnull _Locked self, int flags, const struct timespec* _Nonnull wtp, const struct timespec* _Nonnull itp, dispatch_item_t _Nonnull item)
{
    dispatch_timer_t timer;

    // Make sure that we got at least one worker
    if (_dispatch_ensure_worker_capacity(self, _DISPATCH_EWC_TIMER) != 0) {
        return -1;
    }


    if (self->timer_cache.first) {
        timer = (dispatch_timer_t)SList_RemoveFirst(&self->timer_cache);
        self->timer_cache_count--;
    }
    else {
        timer = malloc(sizeof(struct dispatch_timer));
        if (timer == NULL) {
            return -1;
        }
    }


    item->qe = SLISTNODE_INIT;
    item->state = DISPATCH_STATE_SCHEDULED;
    item->flags &= ~_DISPATCH_ITEM_FLAG_CANCELLED;

    timer->timer_qe = SLISTNODE_INIT;
    timer->item = item;
    timer->deadline = *wtp;
    timer->interval = *itp;

    if ((flags & DISPATCH_SUBMIT_ABSTIME) == 0) {
        struct timespec now;

        clock_gettime(CLOCK_MONOTONIC, &now);
        timespec_add(&now, &timer->deadline, &timer->deadline);
    }


    _dispatch_queue_timer(self, timer);


    // Notify all workers.
    // XXX improve this. Not ideal that we might cause a wakeup storm where we
    // XXX wake up all workers though only one is needed to execute the timer.
    _dispatch_wakeup_all_workers(self);

    return 0;
}

void _dispatch_rearm_timer(dispatch_t _Nonnull _Locked self, dispatch_timer_t _Nonnull timer)
{
    struct timespec now;

    // Repeating timer: rearm it with the next fire date that's in
    // the future (the next fire date we haven't already missed).
    clock_gettime(CLOCK_MONOTONIC, &now);
    do  {
        timespec_add(&timer->deadline, &timer->interval, &timer->deadline);
    } while (timespec_le(&timer->deadline, &now) && timespec_gt(&timer->interval, &TIMESPEC_ZERO));
    

    timer->timer_qe = SLISTNODE_INIT;
    timer->item->state = DISPATCH_STATE_SCHEDULED;
    timer->item->flags &= ~_DISPATCH_ITEM_FLAG_CANCELLED;


    _dispatch_queue_timer(self, timer);

    // No need to wake up workers because this is getting called from a worker
    // and thus at least one worker is clearly alive.
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
    if (item->state == DISPATCH_STATE_IDLE || item->state == DISPATCH_STATE_FINISHED || item->state == DISPATCH_STATE_CANCELLED) {
        if (_dispatch_isactive(self)) {
            item->type = _DISPATCH_TYPE_USER_TIMER;
            item->flags = 0;

            r = _dispatch_arm_timer(self, flags, wtp, &TIMESPEC_INF, item);
        }
    }
    else {
        errno = EBUSY;
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
    if (item->state == DISPATCH_STATE_IDLE || item->state == DISPATCH_STATE_FINISHED || item->state == DISPATCH_STATE_CANCELLED) {
        if (_dispatch_isactive(self)) {
            item->type = _DISPATCH_TYPE_USER_TIMER;
            item->flags = _DISPATCH_ITEM_FLAG_REPEATING;

            r = _dispatch_arm_timer(self, flags, wtp, itp, item);
        }
    }
    else {
        errno = EBUSY;
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
    
        if (item) {
            item->super.type = _DISPATCH_TYPE_CONV_TIMER;
            item->super.flags = _DISPATCH_ITEM_FLAG_CACHEABLE;
            item->func = (int (*)(void*))func;
            item->arg = arg;

            r = _dispatch_arm_timer(self, flags, wtp, &TIMESPEC_INF, (dispatch_item_t)item);
            if (r == -1) {
                _dispatch_cache_item(self, (dispatch_item_t)item);
            }
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

        if (item) {
            item->super.type = _DISPATCH_TYPE_CONV_TIMER;
            item->super.flags = _DISPATCH_ITEM_FLAG_CACHEABLE | _DISPATCH_ITEM_FLAG_REPEATING;
            item->func = (int (*)(void*))func;
            item->arg = arg;

            r = _dispatch_arm_timer(self, flags, wtp, itp, (dispatch_item_t)item);
            if (r == -1) {
                _dispatch_cache_item(self, (dispatch_item_t)item);
            }
        }
    }
    mtx_unlock(&self->mutex);

    return r;
}
