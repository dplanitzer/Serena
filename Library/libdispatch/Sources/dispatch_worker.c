//
//  dispatch_worker.c
//  libdispatch
//
//  Created by Dietmar Planitzer on 7/10/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "dispatch_priv.h"
#include <errno.h>
#include <stdlib.h>
#include <time.h>

#define VP_PRIORITIES_RESERVED_LOW  2


static bool _dispatch_worker_acquire_vcpu(dispatch_worker_t _Nonnull self)
{
    dispatch_t owner = self->owner;

    vcpu_attr_t r_attr;
    r_attr.func = (vcpu_func_t)_dispatch_worker_run;
    r_attr.arg = self;
    r_attr.stack_size = 0;
    r_attr.groupid = owner->groupid;
    r_attr.priority = owner->attr.qos * DISPATCH_PRI_COUNT + (owner->attr.priority + DISPATCH_PRI_COUNT / 2) + VP_PRIORITIES_RESERVED_LOW;
    r_attr.flags = 0;

    self->vcpu = vcpu_acquire(&r_attr);
    if (self->vcpu) {
        self->id = vcpu_id(self->vcpu);

        vcpu_resume(self->vcpu);
        return true;
    }

    return false;
}

static void _dispatch_worker_adopt_current_vcpu(dispatch_worker_t _Nonnull self)
{
    self->vcpu = vcpu_self();
    self->id = vcpu_id(self->vcpu);
}


dispatch_worker_t _Nullable _dispatch_worker_create(dispatch_t _Nonnull owner, int ownership)
{
    dispatch_worker_t self = calloc(1, sizeof(struct dispatch_worker));

    if (self) {
        self->cb = owner->attr.cb;
        self->owner = owner;
        self->ownership = ownership;

        sigemptyset(&self->hotsigs);
        sigaddset(&self->hotsigs, SIGDISPATCH);


        switch (ownership) {
            case _DISPATCH_ACQUIRE_VCPU:
                if (!_dispatch_worker_acquire_vcpu(self)) {
                    free(self);
                    return NULL;
                }
                break;

            case _DISPATCH_ADOPT_VCPU:
                _dispatch_worker_adopt_current_vcpu(self);
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
        self->cb = NULL;
        // vcpu is relinquished by _dispatch_relinquish_worker()
        free(self);
    }
}


////////////////////////////////////////////////////////////////////////////////
// MARK: SPI

void _dispatch_worker_wakeup(dispatch_worker_t _Nonnull _Locked self)
{
    sigsend(SIG_SCOPE_VCPU, self->id, SIGDISPATCH);
}

void _dispatch_worker_submit(dispatch_worker_t _Nonnull _Locked self, dispatch_item_t _Nonnull item)
{
    if (self->cb->insert_item == NULL) {
        SList_InsertAfterLast(&self->work_queue, &item->qe);
    }
    else {
        self->cb->insert_item(item, &self->work_queue);
    }
    self->work_count++;

    _dispatch_worker_wakeup(self);
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

// Get more work for the caller. Returns 0 if work is available and != 0 if
// there is no more work and the worker should relinquish itself. 
static int _get_next_work(dispatch_worker_t _Nonnull _Locked self, mutex_t* _Nonnull mp, dispatch_work_t* _Nonnull wp)
{
    dispatch_t q = self->owner;
    bool mayRelinquish = false;
    struct timespec now, deadline;
    dispatch_item_t item;
    int flags;
    siginfo_t si;

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
        if (self->cb->remove_item == NULL) {
            item = (dispatch_item_t) SList_RemoveFirst(&self->work_queue);
        }
        else {
            item = self->cb->remove_item(&self->work_queue);
        }

        if (item) {
            self->work_count--;
            wp->timer = NULL;
            wp->item = item;

            return 0;
        }

        if (q->state > _DISPATCHER_STATE_ACTIVE && self->work_count == 0) {
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
        } else {
            timespec_from_sec(&deadline, 2);
            flags = 0;
        }


        // Wait for work. This drops the queue lock while we're waiting. This
        // call may return with a ETIMEDOUT error. This is fine. Either some
        // new work has arrived in the meantime or if not then we are free
        // to relinquish the VP since it hasn't done anything useful for a
        // longer time.
        mutex_unlock(mp);
        const int err = sigtimedwait(&self->hotsigs, flags, &deadline, &si);
        mutex_lock(mp);

        if (err == ETIMEDOUT && q->worker_count > q->attr.minConcurrency) {
            mayRelinquish = true;
        }
    }
}

void _dispatch_worker_run(dispatch_worker_t _Nonnull self)
{
    mutex_t* mp = &(self->owner->mutex);

    vcpu_setspecific(__os_dispatch_key, self);

    mutex_lock(mp);

    while (!_get_next_work(self, mp, &self->current)) {
        dispatch_timer_t timer = self->current.timer;
        dispatch_item_t item = self->current.item;

        item->state = DISPATCH_STATE_EXECUTING;
        mutex_unlock(mp);


        item->func(item);


        mutex_lock(mp);
        if (item->state != DISPATCH_STATE_CANCELLED) {
            item->state = DISPATCH_STATE_DONE;
        }


        if (timer) {
            if ((item->flags & _DISPATCH_FLAG_REPEATING) != 0
                && item->state != DISPATCH_STATE_CANCELLED
                && self->owner->state == _DISPATCHER_STATE_ACTIVE) {
                _dispatch_rearm_timer(self->owner, timer);
            }
            else {
                _dispatch_retire_timer(self->owner, timer);
            }
        }
        else {
            _dispatch_retire_item(self->owner, item);
        }
    }

    // Takes care of unlocking 'mp'
    _dispatch_relinquish_worker(self->owner, self);
}
