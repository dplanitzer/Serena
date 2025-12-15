//
//  dispatch_worker.c
//  libdispatch
//
//  Created by Dietmar Planitzer on 7/10/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "dispatch_priv.h"
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <time.h>


static bool _dispatch_worker_acquire_vcpu(dispatch_worker_t _Nonnull self)
{
    dispatch_t owner = self->owner;

    vcpu_attr_t r_attr;
    r_attr.func = (vcpu_func_t)_dispatch_worker_run;
    r_attr.arg = self;
    r_attr.stack_size = 0;
    r_attr.groupid = owner->groupid;
    r_attr.sched_params.type = SCHED_PARAM_QOS;
    r_attr.sched_params.u.qos.category = owner->attr.qos;
    r_attr.sched_params.u.qos.priority = owner->attr.priority;
    r_attr.flags = 0;

    self->allow_relinquish = !_dispatch_is_fixed_concurrency(owner);
    self->vcpu = vcpu_acquire(&r_attr);
    if (self->vcpu) {
        self->id = vcpu_id(self->vcpu);

        vcpu_resume(self->vcpu);
        return true;
    }

    return false;
}

static void _dispatch_worker_adopt_main_vcpu(dispatch_worker_t _Nonnull self)
{
    //XXX not allowing the main vcpu to relinquish for now. Should revisit in the future and enable this
    self->allow_relinquish = false;
    self->vcpu = vcpu_main();
    self->id = vcpu_id(self->vcpu);
}


dispatch_worker_t _Nullable _dispatch_worker_create(dispatch_t _Nonnull owner, int adoption)
{
    dispatch_worker_t self = calloc(1, sizeof(struct dispatch_worker));

    if (self) {
        self->owner = owner;
        self->adoption = adoption;
        self->hotsigs = _SIGBIT(SIGDISP);


        switch (adoption) {
            case _DISPATCH_ACQUIRE_VCPU:
                if (!_dispatch_worker_acquire_vcpu(self)) {
                    free(self);
                    return NULL;
                }
                break;

            case _DISPATCH_ADOPT_MAIN_VCPU:
                _dispatch_worker_adopt_main_vcpu(self);
                break;

            default:
                abort();
        }
    }

    return self;
}

void _dispatch_worker_destroy(dispatch_worker_t _Nullable self)
{
    if (self) {
        self->owner = NULL;
        // vcpu is relinquished by _dispatch_relinquish_worker()
        free(self);
    }
}


////////////////////////////////////////////////////////////////////////////////
// MARK: SPI

void _dispatch_worker_wakeup(dispatch_worker_t _Nonnull _Locked self)
{
    sigsend(SIG_SCOPE_VCPU, self->id, SIGDISP);
}

void _dispatch_worker_submit(dispatch_worker_t _Nonnull _Locked self, dispatch_item_t _Nonnull item, bool doWakeup)
{
    SList_InsertAfterLast(&self->work_queue, &item->qe);
    self->work_count++;

    if (doWakeup) {
        _dispatch_worker_wakeup(self);
    }
}

// Cancels all items that are still on the worker's work queue
void _dispatch_worker_drain(dispatch_worker_t _Nonnull _Locked self)
{
    while (!SList_IsEmpty(&self->work_queue)) {
        dispatch_item_t cip = (dispatch_item_t)SList_RemoveFirst(&self->work_queue);

        _dispatch_retire_item(self->owner, cip);
    }

    self->work_queue = SLIST_INIT;
    self->work_count = 0;
}

// Removes 'item' from the item queue and retires it.
bool _dispatch_worker_withdraw_item(dispatch_worker_t _Nonnull self, dispatch_item_t _Nonnull item)
{
    dispatch_item_t pip = NULL;
    bool foundIt = false;

    SList_ForEach(&self->work_queue, SListNode, {
        dispatch_item_t cip = (dispatch_item_t)pCurNode;

        if (cip == item) {
            foundIt = true;
            break;
        }

        pip = cip;
    });


    if (foundIt) {
        if (pip) {
            SList_Remove(&self->work_queue, &pip->qe, &item->qe);
        }
        else {
            SList_RemoveFirst(&self->work_queue);
        }
        self->work_count--;

        _dispatch_retire_item(self->owner, item);
    }
    return foundIt;
}

dispatch_item_t _Nullable _dispatch_worker_find_item(dispatch_worker_t _Nonnull self, dispatch_item_func_t _Nonnull func, void* _Nullable arg)
{
    SList_ForEach(&self->work_queue, ListNode, {
        dispatch_item_t cip = (dispatch_item_t)pCurNode;

        if (_dispatch_item_has_func(cip, func, arg)) {
             return cip;
        }
    });

    return NULL;
}


////////////////////////////////////////////////////////////////////////////////
// MARK: Work Loop

static void _wait_for_resume(dispatch_worker_t _Nonnull _Locked self)
{
    dispatch_t q = self->owner;
    int signo;

    self->is_suspended = true;
    cnd_broadcast(&q->cond);

    while (q->state == _DISPATCHER_STATE_SUSPENDING || q->state == _DISPATCHER_STATE_SUSPENDED) {
        mtx_unlock(&q->mutex);
        sigtimedwait(&self->hotsigs, 0, &TIMESPEC_INF, &signo);
        mtx_lock(&q->mutex);
    }

    self->is_suspended = false;
}

static bool _should_relinquish(dispatch_worker_t _Nonnull _Locked self)
{
    if (!self->allow_relinquish) {
        return false;
    }


    dispatch_t q = self->owner;
    const int has_armed_sigs = (self->hotsigs & ~_SIGBIT(SIGDISP)) != 0;
    const int has_armed_timers = q->timers.first != NULL;

    if (!has_armed_sigs && !has_armed_timers && q->worker_count > q->attr.minConcurrency) {
        return true;
    }

    // Keep at least one worker alive if signal handlers or timers are armed
    if ((has_armed_sigs || has_armed_timers) && q->worker_count > q->attr.minConcurrency && q->worker_count > 1) {
        return true;
    }

    return false;
}

// Get more work for the caller. Returns 0 if work is available and != 0 if
// there is no more work and the worker should relinquish itself. 
static int _get_next_work(dispatch_worker_t _Nonnull _Locked self)
{
    dispatch_t q = self->owner;
    bool mayRelinquish = false;
    struct timespec now, deadline;
    int flags, signo;

    for (;;) {
        // Grab the first timer that's due. We give preference to timers because
        // they are tied to a specific deadline time while immediate work items
        // do not guarantee that they will execute at a specific time. So it's
        // acceptable to push them back on the timeline.
        dispatch_timer_t tp = (dispatch_timer_t)q->timers.first;
        if (tp) {
            clock_gettime(CLOCK_MONOTONIC, &now);

            if (timespec_le(&tp->deadline, &now)) {
                SList_RemoveFirst(&q->timers);
                self->current_item = tp->item;
                self->current_timer = tp;

                return 0;
            }
        }


        // Next grab a work item if there's one queued
        dispatch_item_t ip = (dispatch_item_t) SList_RemoveFirst(&self->work_queue);
        if (ip) {
            self->work_count--;
        }
        else if (q->worker_count > 1) {
            // Try stealing a work item (aka rebalancing) from another worker
            ip = _dispatch_steal_work_item(q);
        }
        if (ip) {
            self->current_item = ip;
            self->current_timer = NULL;

            return 0;
        }


        if (q->state >= _DISPATCHER_STATE_TERMINATING && self->work_count == 0) {
            return 1;
        }
        if (mayRelinquish) {
            return 1;
        }


        // Compute a deadline for the wait. We do not wait if the deadline
        // is equal to the current time or it's in the past
        if (q->timers.first) {
            deadline = ((dispatch_timer_t)q->timers.first)->deadline;
            flags = TIMER_ABSTIME;
        }
        else if (self->allow_relinquish) {
            timespec_from_sec(&deadline, 5);
            flags = 0;
        }
        else {
            deadline = TIMESPEC_INF;
        }


        // Wait for work. This drops the queue lock while we're waiting. This
        // call may return with a ETIMEDOUT error. This is fine. Either some
        // new work has arrived in the meantime or if not then we are free
        // to relinquish the VP since it hasn't done anything useful for a
        // longer time.
        mtx_unlock(&q->mutex);
        const int r = sigtimedwait(&self->hotsigs, flags, &deadline, &signo);
        mtx_lock(&q->mutex);

        if (r != 0 && errno == ETIMEDOUT && _should_relinquish(self)) {
            mayRelinquish = true;
        }

        if (q->state == _DISPATCHER_STATE_SUSPENDING || q->state == _DISPATCHER_STATE_SUSPENDED) {
            _wait_for_resume(self);
        }

        if (signo != SIGDISP) {
            _dispatch_submit_items_for_signal(q, signo, self);
        }
    }
}

void _dispatch_worker_run(dispatch_worker_t _Nonnull self)
{
    dispatch_t q = self->owner;

    vcpu_setspecific(__os_dispatch_key, self);

    mtx_lock(&q->mutex);

    while (!_get_next_work(self)) {
        dispatch_item_t ip = self->current_item;

        ip->state = DISPATCH_STATE_EXECUTING;
        mtx_unlock(&q->mutex);


        ip->func(ip);


        mtx_lock(&q->mutex);
        switch (ip->type) {
            case _DISPATCH_TYPE_USER_ITEM:
            case _DISPATCH_TYPE_CONV_ITEM:
                _dispatch_retire_item(q, ip);
                break;

            case _DISPATCH_TYPE_USER_SIGNAL_ITEM:
                if ((ip->flags & _DISPATCH_ITEM_FLAG_REPEATING) != 0
                    && (ip->flags & _DISPATCH_ITEM_FLAG_CANCELLED) == 0) {
                    _dispatch_rearm_signal_item(q, ip);
                }
                else {
                    _dispatch_retire_signal_item(q, ip);
                }
                break;

            case _DISPATCH_TYPE_USER_TIMER:
            case _DISPATCH_TYPE_CONV_TIMER:
                if ((ip->flags & _DISPATCH_ITEM_FLAG_REPEATING) != 0
                    && (ip->flags & _DISPATCH_ITEM_FLAG_CANCELLED) == 0) {
                    _dispatch_rearm_timer(q, self->current_timer);
                }
                else {
                    _dispatch_retire_timer(q, self->current_timer);
                }
                break;

            default:
                abort();
        }

        self->current_item = NULL;
        self->current_timer = NULL;
    }

    // Takes care of unlocking 'mp'
    _dispatch_relinquish_worker(q, self);
}
