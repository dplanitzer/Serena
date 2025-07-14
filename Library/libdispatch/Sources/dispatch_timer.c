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

void _dispatch_retire_timer(dispatch_t _Nonnull _Locked self, dispatch_timer_t _Nonnull timer)
{
    _dispatch_retire_item(self, timer->item);
    timer->item = NULL;
    _dispatch_cache_timer(self, timer);
}

void _dispatch_drain_timers(dispatch_t _Nonnull _Locked self)
{
    while (!SList_IsEmpty(&self->timers)) {
        dispatch_timer_t ctp = (dispatch_timer_t)SList_RemoveFirst(&self->timers);

        _dispatch_retire_timer(self, ctp);
    }
}

void _dispatch_cancel_timer(dispatch_t _Nonnull self, int flags, dispatch_item_t _Nonnull item)
{
    dispatch_timer_t ptp = NULL;
    dispatch_timer_t ctp = (dispatch_timer_t)self->timers.first;

    while (ctp) {
        dispatch_timer_t ntp = (dispatch_timer_t)ctp->timer_qe.next;

        if (ctp->item == item) {
            SList_Remove(&self->timers, &ptp->timer_qe, &ctp->timer_qe);
            ctp->item->state = DISPATCH_STATE_CANCELLED;
            _dispatch_retire_timer(self, ctp);
            break;
        }

        ptp = ctp;
        ctp = ntp;
    }
}

dispatch_timer_t _Nullable _dispatch_find_timer(dispatch_t _Nonnull self, dispatch_item_func_t _Nonnull func)
{
    SList_ForEach(&self->timers, ListNode, {
        dispatch_timer_t ctp = (dispatch_timer_t)pCurNode;

        if (ctp->item->func == func) {
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


    timer->item->qe = SLISTNODE_INIT;
    timer->item->flags |= _DISPATCH_ITEM_TIMED;
    timer->item->state = DISPATCH_STATE_PENDING;


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
    
    timer->timer_qe = SLISTNODE_INIT;
    timer->item->state = DISPATCH_STATE_IDLE;

    do  {
        timespec_add(&timer->deadline, &timer->interval, &timer->deadline);
    } while (timespec_le(&timer->deadline, &now) && timespec_gt(&timer->interval, &TIMESPEC_ZERO));
    
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
        item->flags |= _DISPATCH_ITEM_RESUBMIT;
    }
    else {
        item->flags &= ~_DISPATCH_ITEM_RESUBMIT;
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

int _dispatch_convenience_timer(dispatch_t _Nonnull self, int flags, const struct timespec* _Nonnull wtp, const struct timespec* _Nullable itp, dispatch_async_func_t _Nonnull func, void* _Nullable context)
{
    int r = -1;

    mutex_lock(&self->mutex);
    if (self->state == _DISPATCHER_STATE_ACTIVE) {
       dispatch_cacheable_item_t item = _dispatch_acquire_cached_item(self, sizeof(struct dispatch_async_item), _async_adapter_func, _DISPATCH_ITEM_CACHEABLE);
    
        if (item) {
            ((dispatch_async_item_t)item)->func = func;
            ((dispatch_async_item_t)item)->context = context;
            r = _dispatch_timer(self, (dispatch_item_t)item, flags, wtp, itp);
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

int dispatch_after(dispatch_t _Nonnull self, int flags, const struct timespec* _Nonnull wtp, dispatch_async_func_t _Nonnull func, void* _Nullable context)
{
    return _dispatch_convenience_timer(self, flags, wtp, NULL, func, context);
}

int dispatch_repeating(dispatch_t _Nonnull self, int flags, const struct timespec* _Nonnull wtp, const struct timespec* _Nonnull itp, dispatch_async_func_t _Nonnull func, void* _Nullable context)
{
    return _dispatch_convenience_timer(self, flags, wtp, itp, func, context);
}
