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

    self->allow_relinquish = true;
    self->vcpu = vcpu_acquire(&r_attr);
    if (self->vcpu) {
        self->id = vcpu_id(self->vcpu);

        vcpu_resume(self->vcpu);
        return true;
    }

    return false;
}

static void _dispatch_worker_adopt_caller_vcpu(dispatch_worker_t _Nonnull self)
{
    //XXX not allowing the main vcpu to relinquish for now. Should revisit in the future and enable this
    self->allow_relinquish = false;
    self->vcpu = vcpu_self();
    self->id = vcpu_id(self->vcpu);
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

        sigemptyset(&self->hotsigs);
        sigaddset(&self->hotsigs, SIGDISP);


        switch (adoption) {
            case _DISPATCH_ACQUIRE_VCPU:
                if (!_dispatch_worker_acquire_vcpu(self)) {
                    free(self);
                    return NULL;
                }
                break;

            case _DISPATCH_ADOPT_CALLER_VCPU:
                _dispatch_worker_adopt_caller_vcpu(self);
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

bool _dispatch_worker_cancel_item(dispatch_worker_t _Nonnull self, int flags, dispatch_item_t _Nonnull item)
{
    dispatch_item_t pip = NULL;

    SList_ForEach(&self->work_queue, ListNode, {
        dispatch_item_t cip = (dispatch_item_t)pCurNode;

        if (cip == item) {
            SList_Remove(&self->work_queue, &pip->qe, &cip->qe);
            _dispatch_retire_item(self->owner, cip);
            return true;
        }

        pip = cip;
    });

    return false;
}

dispatch_item_t _Nullable _dispatch_worker_find_item(dispatch_worker_t _Nonnull self, dispatch_item_func_t _Nonnull func)
{
    SList_ForEach(&self->work_queue, ListNode, {
        dispatch_item_t cip = (dispatch_item_t)pCurNode;

        if (cip->func == func) {
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

// Get more work for the caller. Returns 0 if work is available and != 0 if
// there is no more work and the worker should relinquish itself. 
static int _get_next_work(dispatch_worker_t _Nonnull _Locked self, dispatch_work_t* _Nonnull wp)
{
    dispatch_t q = self->owner;
    bool mayRelinquish = false;
    struct timespec now, deadline;
    dispatch_item_t item;
    int flags, signo;

    for (;;) {
        // Grab the first timer that's due. We give preference to timers because
        // they are tied to a specific deadline time while immediate work items
        // do not guarantee that they will execute at a specific time. So it's
        // acceptable to push them back on the timeline.
        dispatch_timer_t ftp = (dispatch_timer_t)q->timers.first;
        if (ftp) {
            clock_gettime(CLOCK_MONOTONIC, &now);

            if (timespec_le(&ftp->deadline, &now)) {
                SList_RemoveFirst(&q->timers);
                wp->timer = ftp;
                wp->item = ftp->item;

                return 0;
            }
        }


        // Next grab a work item if there's one queued
        item = (dispatch_item_t) SList_RemoveFirst(&self->work_queue);
        if (item) {
            self->work_count--;
            wp->timer = NULL;
            wp->item = item;

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
            timespec_from_sec(&deadline, 2);
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

        if (r != 0 && q->worker_count > q->attr.minConcurrency && self->allow_relinquish && (self->hotsigs & ~SIGDISP) == 0 && errno == ETIMEDOUT) {
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

    while (!_get_next_work(self, &self->current)) {
        dispatch_item_t item = self->current.item;

        item->state = DISPATCH_STATE_EXECUTING;
        mtx_unlock(&q->mutex);


        item->func(item);


        mtx_lock(&q->mutex);
        if (item->state != DISPATCH_STATE_CANCELLED) {
            item->state = DISPATCH_STATE_DONE;
        }


        switch (item->type) {
            case _DISPATCH_TYPE_WORK_ITEM:
                _dispatch_retire_item(q, item);
                break;

            case _DISPATCH_TYPE_SIGNAL_ITEM:
                if ((item->flags & _DISPATCH_SUBMIT_REPEATING) != 0
                    && item->state != DISPATCH_STATE_CANCELLED) {
                    _dispatch_rearm_signal_item(q, item);
                }
                else {
                    _dispatch_cancel_signal_item(q, 0, item);
                }
                break;

            case _DISPATCH_TYPE_TIMED_ITEM: {
                dispatch_timer_t timer = self->current.timer;

                if ((item->flags & _DISPATCH_SUBMIT_REPEATING) != 0
                    && item->state != DISPATCH_STATE_CANCELLED) {
                    _dispatch_rearm_timer(q, timer);
                }
                else {
                    _dispatch_retire_timer(q, timer);
                }
            }
                break;

            default:
                abort();
        }
    }

    // Takes care of unlocking 'mp'
    _dispatch_relinquish_worker(q, self);
}
