//
//  kdispatch_worker.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/10/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "kdispatch_priv.h"
#include <process/Process.h>


static errno_t _kdispatch_worker_acquire_vcpu(kdispatch_worker_t _Nonnull self)
{
    decl_try_err();
    kdispatch_t owner = self->owner;

    _vcpu_acquire_attr_t attr;
    attr.func = (vcpu_func_t)_kdispatch_worker_run;
    attr.arg = self;
    attr.stack_size = 0;
    attr.groupid = VCPUID_MAIN_GROUP;
    attr.sched_params.type = SCHED_PARAM_QOS;
    attr.sched_params.u.qos.category = owner->attr.qos;
    attr.sched_params.u.qos.priority = owner->attr.priority;
    attr.flags = 0;
    attr.data = 0;

    self->allow_relinquish = true;
    err = Process_AcquireVirtualProcessor(gKernelProcess, &attr, &self->vcpu);
    if (err == EOK) {

        vcpu_resume(self->vcpu, false);
        return EOK;
    }

    return err;
}


errno_t _kdispatch_worker_create(kdispatch_t _Nonnull owner, kdispatch_worker_t _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    kdispatch_worker_t self = NULL;
    
    try(kalloc_cleared(sizeof(struct kdispatch_worker), (void**)&self));

    self->owner = owner;

    wq_init(&self->wq);

    sigemptyset(&self->hotsigs);
    sigaddset(&self->hotsigs, SIGDISP);

    try(_kdispatch_worker_acquire_vcpu(self));
    self->vcpu->udata = (intptr_t)self;

catch:
    if (err != EOK) {
        kfree(self);
    }
    *pOutSelf = self;

    return err;
}

void _kdispatch_worker_destroy(kdispatch_worker_t _Nullable self)
{
    if (self) {
        self->owner = NULL;
        // vcpu is relinquished by _kdispatch_relinquish_worker()
        kfree(self);
    }
}


////////////////////////////////////////////////////////////////////////////////
// MARK: SPI

void _kdispatch_worker_wakeup(kdispatch_worker_t _Nonnull _Locked self)
{
    vcpu_sigsend(self->vcpu, SIGDISP);
}

void _kdispatch_worker_submit(kdispatch_worker_t _Nonnull _Locked self, kdispatch_item_t _Nonnull item, bool doWakeup)
{
    SList_InsertAfterLast(&self->work_queue, &item->qe);
    self->work_count++;

    if (doWakeup) {
        _kdispatch_worker_wakeup(self);
    }
}

// Cancels all items that are still on the worker's work queue
void _kdispatch_worker_drain(kdispatch_worker_t _Nonnull _Locked self)
{
    while (!SList_IsEmpty(&self->work_queue)) {
        kdispatch_item_t cip = (kdispatch_item_t)SList_RemoveFirst(&self->work_queue);

        _kdispatch_retire_item(self->owner, cip);
    }

    self->work_queue = SLIST_INIT;
    self->work_count = 0;
}

// Removes 'item' from the item queue and retires it.
bool _kdispatch_worker_withdraw_item(kdispatch_worker_t _Nonnull self, kdispatch_item_t _Nonnull item)
{
    kdispatch_item_t pip = NULL;

    SList_ForEach(&self->work_queue, SListNode, {
        kdispatch_item_t cip = (kdispatch_item_t)pCurNode;

        if (cip == item) {
            SList_Remove(&self->work_queue, &pip->qe, &cip->qe);
            _kdispatch_retire_item(self->owner, cip);
            return true;
        }

        pip = cip;
    });

    return false;
}

kdispatch_item_t _Nullable _kdispatch_worker_find_item(kdispatch_worker_t _Nonnull self, kdispatch_item_func_t _Nonnull func, void* _Nullable arg)
{
    SList_ForEach(&self->work_queue, ListNode, {
        kdispatch_item_t cip = (kdispatch_item_t)pCurNode;

        if (_kdispatch_item_has_func(cip, func, arg)) {
             return cip;
        }
    });

    return NULL;
}


////////////////////////////////////////////////////////////////////////////////
// MARK: Work Loop

static void _wait_for_resume(kdispatch_worker_t _Nonnull _Locked self)
{
    kdispatch_t q = self->owner;
    int signo;

    self->is_suspended = true;
    cnd_broadcast(&q->cond);

    while (q->state == _DISPATCHER_STATE_SUSPENDING || q->state == _DISPATCHER_STATE_SUSPENDED) {
        mtx_unlock(&q->mutex);
        vcpu_sigtimedwait(&self->wq, &self->hotsigs, 0, &TIMESPEC_INF, &signo);
        mtx_lock(&q->mutex);
    }

    self->is_suspended = false;
}

static bool _should_relinquish(kdispatch_worker_t _Nonnull _Locked self)
{
    if (!self->allow_relinquish) {
        return false;
    }


    kdispatch_t q = self->owner;
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
static int _get_next_work(kdispatch_worker_t _Nonnull _Locked self)
{
    kdispatch_t q = self->owner;
    bool mayRelinquish = false;
    struct timespec now, deadline;
    kdispatch_item_t item;
    int flags, signo;

    self->current_item = NULL;
    self->current_timer = NULL;

    for (;;) {
        // Grab the first timer that's due. We give preference to timers because
        // they are tied to a specific deadline time while immediate work items
        // do not guarantee that they will execute at a specific time. So it's
        // acceptable to push them back on the timeline.
        kdispatch_timer_t ftp = (kdispatch_timer_t)q->timers.first;
        if (ftp) {
            clock_gettime(g_mono_clock, &now);

            if (timespec_le(&ftp->deadline, &now)) {
                SList_RemoveFirst(&q->timers);
                self->current_item = ftp->item;
                self->current_timer = ftp;

                return 0;
            }
        }


        // Next grab a work item if there's one queued
        item = (kdispatch_item_t) SList_RemoveFirst(&self->work_queue);
        if (item == NULL) {
            // Try stealing a work item (aka rebalancing) from another worker
            item = _kdispatch_steal_work_item(self->owner);
        }
        if (item) {
            self->work_count--;
            self->current_item = item;
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
            deadline = ((kdispatch_timer_t)q->timers.first)->deadline;
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
        const errno_t err = vcpu_sigtimedwait(&self->wq, &self->hotsigs, flags, &deadline, &signo);
        mtx_lock(&q->mutex);

        if (err == ETIMEDOUT && _should_relinquish(self)) {
            mayRelinquish = true;
        }

        if (q->state == _DISPATCHER_STATE_SUSPENDING || q->state == _DISPATCHER_STATE_SUSPENDED) {
            _wait_for_resume(self);
        }

        if (signo != SIGDISP) {
            _kdispatch_submit_items_for_signal(q, signo, self);
        }
    }
}

void _kdispatch_worker_run(kdispatch_worker_t _Nonnull self)
{
    kdispatch_t q = self->owner;

    mtx_lock(&q->mutex);

    while (!_get_next_work(self)) {
        kdispatch_item_t item = self->current_item;

        item->state = KDISPATCH_STATE_EXECUTING;
        mtx_unlock(&q->mutex);


        item->func(item);


        mtx_lock(&q->mutex);
        switch (item->type) {
            case _KDISPATCH_TYPE_USER_ITEM:
            case _KDISPATCH_TYPE_CONV_ITEM:
                _kdispatch_retire_item(q, item);
                break;

            case _KDISPATCH_TYPE_USER_SIGNAL_ITEM:
                if ((item->flags & _KDISPATCH_ITEM_FLAG_REPEATING) != 0
                    && (item->flags & _KDISPATCH_ITEM_FLAG_CANCELLED) == 0) {
                    _kdispatch_rearm_signal_item(q, item);
                }
                else {
                    _kdispatch_retire_signal_item(q, item);
                }
                break;

            case _KDISPATCH_TYPE_USER_TIMER:
            case _KDISPATCH_TYPE_CONV_TIMER:
                if ((item->flags & _KDISPATCH_ITEM_FLAG_REPEATING) != 0
                    && (item->flags & _KDISPATCH_ITEM_FLAG_CANCELLED) == 0) {
                    _kdispatch_rearm_timer(q, self->current_timer);
                }
                else {
                    _kdispatch_retire_timer(q, self->current_timer);
                }
                break;
            
            default:
                abort();
        }
    }

    // Takes care of unlocking 'mp'
    _kdispatch_relinquish_worker(q, self);
}
