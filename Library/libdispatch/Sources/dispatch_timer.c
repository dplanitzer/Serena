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


void _dispatch_drain_timers(dispatch_t _Nonnull _Locked self)
{
    while (!SList_IsEmpty(&self->timers)) {
        dispatch_item_t ctp = (dispatch_item_t)SList_RemoveFirst(&self->timers);

        _dispatch_retire_item(self, ctp);
    }
}

// Removes 'item' from the timer queue and retires it.
void _dispatch_withdraw_timer(dispatch_t _Nonnull self, int flags, dispatch_item_t _Nonnull item)
{
    dispatch_item_t ptp = NULL;
    dispatch_item_t ctp = (dispatch_item_t)self->timers.first;

    while (ctp) {
        dispatch_item_t ntp = (dispatch_item_t)ctp->qe.next;

        if (ctp == item) {
            SList_Remove(&self->timers, &ptp->qe, &ctp->qe);
            _dispatch_retire_item(self, ctp);
            break;
        }

        ptp = ctp;
        ctp = ntp;
    }
}

dispatch_timer_t _Nullable _dispatch_find_timer(dispatch_t _Nonnull self, dispatch_item_func_t _Nonnull func)
{
    SList_ForEach(&self->timers, SListNode, {
        dispatch_timer_t ctp = (dispatch_timer_t)pCurNode;
        bool match;

        switch (ctp->item.type) {
            case _DISPATCH_TYPE_CONV_TIMER:
                match = ((dispatch_conv_timer_t)ctp)->func == (dispatch_async_func_t)func;
                break;

            case _DISPATCH_TYPE_USER_TIMER:
                match = ctp->item.func == func;
                break;

            default:
                match = false;
                break;
        }
        if (match) {
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


    timer->item.qe = SLISTNODE_INIT;
    timer->item.state = DISPATCH_STATE_SCHEDULED;
    timer->item.flags &= ~_DISPATCH_ITEM_FLAG_CANCELLED;


    // Put the timer on the timer queue. The timer queue is sorted by absolute
    // timer fire time (ascending). Timers with the same fire time are added in
    // FIFO order.
    while (ctp) {
        if (timespec_gt(&ctp->deadline, &timer->deadline)) {
            break;
        }
        
        ptp = ctp;
        ctp = (dispatch_timer_t)ctp->item.qe.next;
    }
    
    SList_InsertAfter(&self->timers, &timer->item.qe, &ptp->item.qe);


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
    
    timer->item.qe = SLISTNODE_INIT;
    timer->item.state = DISPATCH_STATE_IDLE;

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

int dispatch_timer(dispatch_t _Nonnull self, int flags, dispatch_timer_t _Nonnull timer)
{
    int r = -1;

    if (!timespec_isvalid(&timer->deadline) || !timespec_isvalid(&timer->interval)) {
        errno = EINVAL;
        return -1;
    }
    if (timer->item.state == DISPATCH_STATE_SCHEDULED || timer->item.state == DISPATCH_STATE_EXECUTING) {
        errno = EBUSY;
        return -1;
    }
    if (timer->item.func == NULL) {
        errno = EINVAL;
        return -1;
    }

    mtx_lock(&self->mutex);
    if (_dispatch_isactive(self)) {
        timer->item.type = _DISPATCH_TYPE_USER_TIMER;
        timer->item.subtype = 0;
        timer->item.flags = 0;

        if (timespec_lt(&timer->interval, &TIMESPEC_INF)) {
            timer->item.flags |= _DISPATCH_ITEM_FLAG_REPEATING;
        }

        _calc_timer_absolute_deadline(timer, flags);
        r = _dispatch_arm_timer(self, timer);
    }
    mtx_unlock(&self->mutex);
    return r;
}

static void _conv_timer_adapter_func(dispatch_item_t _Nonnull item)
{
    dispatch_conv_timer_t tp = (dispatch_conv_timer_t)item;

    tp->func(tp->arg);
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
       dispatch_conv_timer_t timer = (dispatch_conv_timer_t)_dispatch_acquire_cached_item(self, _DISPATCH_TYPE_CONV_TIMER, _conv_timer_adapter_func);
    
        if (timer) {
            timer->timer.item.flags = _DISPATCH_ITEM_FLAG_CACHEABLE;
            timer->func = func;
            timer->arg = arg;
            timer->timer.deadline = *wtp;
            timer->timer.interval = TIMESPEC_INF;
            _calc_timer_absolute_deadline((dispatch_timer_t)timer, flags);

            r = _dispatch_arm_timer(self, (dispatch_timer_t)timer);
            if (r != 0) {
                _dispatch_cache_item(self, (dispatch_item_t)timer);
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
       dispatch_conv_timer_t timer = (dispatch_conv_timer_t)_dispatch_acquire_cached_item(self, _DISPATCH_TYPE_CONV_TIMER, _conv_timer_adapter_func);
    
        if (timer) {
            timer->timer.item.flags = _DISPATCH_ITEM_FLAG_CACHEABLE | _DISPATCH_ITEM_FLAG_REPEATING;
            timer->func = func;
            timer->arg = arg;
            timer->timer.deadline = *wtp;
            timer->timer.interval = *itp;
            _calc_timer_absolute_deadline((dispatch_timer_t)timer, flags);

            r = _dispatch_arm_timer(self, (dispatch_timer_t)timer);
            if (r != 0) {
                _dispatch_cache_item(self, (dispatch_item_t)timer);
            }
        }
    }
    mtx_unlock(&self->mutex);

    return r;
}
