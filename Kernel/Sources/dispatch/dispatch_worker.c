//
//  dispatch_worker.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/10/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "dispatch_priv.h"
#include <process/Process.h>


static errno_t _dispatch_worker_acquire_vcpu(dispatch_worker_t _Nonnull self)
{
    decl_try_err();
    dispatch_t owner = self->owner;

    _vcpu_acquire_attr_t attr;
    attr.func = (vcpu_func_t)_dispatch_worker_run;
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


errno_t _dispatch_worker_create(dispatch_t _Nonnull owner, dispatch_worker_t _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    dispatch_worker_t self = NULL;
    
    try(kalloc_cleared(sizeof(struct dispatch_worker), (void**)&self));

    self->owner = owner;

    wq_init(&self->wq);

    sigemptyset(&self->hotsigs);
    sigaddset(&self->hotsigs, SIGDISP);

    try(_dispatch_worker_acquire_vcpu(self));
    self->vcpu->udata = (intptr_t)self;

catch:
    if (err != EOK) {
        kfree(self);
    }
    *pOutSelf = self;

    return err;
}

void _dispatch_worker_destroy(dispatch_worker_t _Nullable self)
{
    if (self) {
        self->owner = NULL;
        // vcpu is relinquished by _dispatch_relinquish_worker()
        kfree(self);
    }
}


////////////////////////////////////////////////////////////////////////////////
// MARK: SPI

void _dispatch_worker_wakeup(dispatch_worker_t _Nonnull _Locked self)
{
    vcpu_sigsend(self->vcpu, SIGDISP);
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
bool _dispatch_worker_withdraw_item(dispatch_worker_t _Nonnull self, int flags, dispatch_item_t _Nonnull item)
{
    dispatch_item_t pip = NULL;

    SList_ForEach(&self->work_queue, SListNode, {
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

dispatch_item_t _Nullable _dispatch_worker_find_item(dispatch_worker_t _Nonnull self, dispatch_item_func_t _Nonnull func, void* _Nullable arg)
{
    SList_ForEach(&self->work_queue, ListNode, {
        dispatch_item_t cip = (dispatch_item_t)pCurNode;
        bool hasFunc;
        bool hasArg;

        switch (cip->type) {
            case _DISPATCH_TYPE_CONV_ITEM:
                hasFunc = func == (dispatch_item_func_t)((dispatch_conv_item_t)cip)->func;
                hasArg = (arg == DISPATCH_IGNORE_ARG) || (((dispatch_conv_item_t)cip)->arg == arg);
                break;

            case _DISPATCH_TYPE_USER_ITEM:
            case _DISPATCH_TYPE_USER_SIGNAL_ITEM:
                hasFunc = func == cip->func;
                hasArg = true;
                break;

            default:
                hasFunc = false;
                hasArg = false;
                break;
        }

        if (hasFunc && hasArg) {
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
        vcpu_sigtimedwait(&self->wq, &self->hotsigs, 0, &TIMESPEC_INF, &signo);
        mtx_lock(&q->mutex);
    }

    self->is_suspended = false;
}

// Get more work for the caller. Returns 0 if work is available and != 0 if
// there is no more work and the worker should relinquish itself. 
static int _get_next_work(dispatch_worker_t _Nonnull _Locked self)
{
    dispatch_t q = self->owner;
    bool mayRelinquish = false;
    struct timespec now, deadline;
    dispatch_item_t item;
    int flags, signo;

    self->current_item = NULL;
    self->current_timer = NULL;

    for (;;) {
        // Grab the first timer that's due. We give preference to timers because
        // they are tied to a specific deadline time while immediate work items
        // do not guarantee that they will execute at a specific time. So it's
        // acceptable to push them back on the timeline.
        dispatch_timer_t ftp = (dispatch_timer_t)q->timers.first;
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
        item = (dispatch_item_t) SList_RemoveFirst(&self->work_queue);
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
        const errno_t err = vcpu_sigtimedwait(&self->wq, &self->hotsigs, flags, &deadline, &signo);
        mtx_lock(&q->mutex);

        if (err == ETIMEDOUT && q->worker_count > q->attr.minConcurrency && self->allow_relinquish && (self->hotsigs & ~_SIGBIT(SIGDISP)) == 0) {
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

    mtx_lock(&q->mutex);

    while (!_get_next_work(self)) {
        dispatch_item_t item = self->current_item;

        item->state = DISPATCH_STATE_EXECUTING;
        mtx_unlock(&q->mutex);


        item->func(item);


        mtx_lock(&q->mutex);
        switch (item->type) {
            case _DISPATCH_TYPE_USER_ITEM:
            case _DISPATCH_TYPE_CONV_ITEM:
                _dispatch_retire_item(q, item);
                break;

            case _DISPATCH_TYPE_USER_SIGNAL_ITEM:
                if ((item->flags & _DISPATCH_ITEM_FLAG_REPEATING) != 0
                    && (item->flags & _DISPATCH_ITEM_FLAG_CANCELLED) == 0) {
                    _dispatch_rearm_signal_item(q, item);
                }
                else {
                    _dispatch_retire_signal_item(q, item);
                }
                break;

            case _DISPATCH_TYPE_USER_TIMER:
            case _DISPATCH_TYPE_CONV_TIMER:
                if ((item->flags & _DISPATCH_ITEM_FLAG_REPEATING) != 0
                    && (item->flags & _DISPATCH_ITEM_FLAG_CANCELLED) == 0) {
                    _dispatch_rearm_timer(q, self->current_timer);
                }
                else {
                    _dispatch_retire_timer(q, self->current_timer);
                }
                break;
            
            default:
                abort();
        }
    }

    // Takes care of unlocking 'mp'
    _dispatch_relinquish_worker(q, self);
}
