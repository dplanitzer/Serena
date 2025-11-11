//
//  dispatch_signal.c
//  libdispatch
//
//  Created by Dietmar Planitzer on 9/21/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "dispatch_priv.h"
#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>


static void _dispatch_enable_signal(dispatch_t _Nonnull _Locked self, int signo, bool enable)
{
    List_ForEach(&self->workers, ListNode, {
        dispatch_worker_t cwp = (dispatch_worker_t)pCurNode;

        if (enable) {
            sigaddset(&cwp->hotsigs, signo);
        }
        else {
            sigdelset(&cwp->hotsigs, signo);
        }
    });
}

void _dispatch_cancel_signal_item(dispatch_t _Nonnull self, int flags, dispatch_item_t _Nonnull item)
{
    const int signo = item->subtype;
    dispatch_sigmon_t* sm = &self->sigmons[signo - 1];

    _dispatch_retire_item(self, item);

    sm->handlers_count--;
    if (sm->handlers_count == 0) {
        _dispatch_enable_signal(self, signo, false);
    }
}

// Rearms 'item' in the sense that it is moved back to idle state and the signal
// monitor so that it can be submitted again when the next signal comes in.
void _dispatch_rearm_signal_item(dispatch_t _Nonnull _Locked self, dispatch_item_t _Nonnull item)
{
    dispatch_sigmon_t* sm = &self->sigmons[item->subtype - 1];

    item->state = DISPATCH_STATE_IDLE;
    item->qe = SLISTNODE_INIT;

    SList_InsertAfterLast(&sm->handlers, &item->qe);
}

static int _dispatch_signal_monitor(dispatch_t _Nonnull _Locked self, int signo, dispatch_item_t _Nonnull item)
{
    if (self->sigmons == NULL) {
        //XXX allocate in a smarter way: eg organize sigset in quarters, calc
        //XXX what's the highest quarter we need and only allocate up to this
        //XXX quarter.
        self->sigmons = calloc(SIGMAX, sizeof(dispatch_sigmon_t));
        if (self->sigmons == NULL) {
            return -1;
        }
    }

    item->type = _DISPATCH_TYPE_SIGNAL_ITEM;
    item->subtype = (uint8_t)signo;
    item->flags = _DISPATCH_SUBMIT_REPEATING;
    item->state = DISPATCH_STATE_IDLE;
    item->qe = SLISTNODE_INIT;

    dispatch_sigmon_t* sm = &self->sigmons[signo - 1];
    SList_InsertAfterLast(&sm->handlers, &item->qe);
    sm->handlers_count++;

    if (sm->handlers_count == 1) {
        _dispatch_enable_signal(self, signo, true);
    }

    // For now we ensure that there's always at least one worker alive.
    //XXX The kernel should be able to spawns a worker for us in the future if
    //XXX a signal comes in and there's no vcpu in the vcpu group. 
    if (self->worker_count == 0) {
        const int r = _dispatch_acquire_worker(self);

        if (r != 0) {
            return -1;
        }
    }

    return 0;
}

void _dispatch_submit_items_for_signal(dispatch_t _Nonnull _Locked self, int signo, dispatch_worker_t _Nonnull worker)
{
    dispatch_sigmon_t* sm = &self->sigmons[signo - 1];

    while (sm->handlers.first) {
        dispatch_item_t item = (dispatch_item_t)SList_RemoveFirst(&sm->handlers);

        item->state = DISPATCH_STATE_PENDING;
        item->qe = SLISTNODE_INIT;

        // No need to wakeup ourselves. This function is called from the worker
        // 'worker' and we know we're already awake.
        _dispatch_worker_submit(worker, item, false);
    }
}


////////////////////////////////////////////////////////////////////////////////
// MARK: API

int dispatch_signal_monitor(dispatch_t _Nonnull self, int signo, dispatch_item_t _Nonnull item)
{
    int r = -1;

    if (signo < SIGMIN || signo > SIGMAX || signo == SIGDISP || signo == SIGKILL) {
        errno = EINVAL;
        return -1;
    }

    mtx_lock(&self->mutex);
    if (_dispatch_isactive(self)) {
        r = _dispatch_signal_monitor(self, signo, item);
    }
    mtx_unlock(&self->mutex);
    return r;
}

int dispatch_alloc_signal(dispatch_t _Nonnull self, int signo)
{
    int r = -1;

    mtx_lock(&self->mutex);
    if (signo == 0) {
        // Allocate the first lowest priority signal available
        for (int i = SIGMAX; i >= SIGMIN; i--) {
            if (!sigismember(&self->alloced_sigs, i)) {
                sigaddset(&self->alloced_sigs, i);
                r = i;
                break;
            }
        }
        if (r == -1) {
            errno = EBUSY;
        }
    }
    else {
        // Allocate the specific signal 'signo'
        if (!sigismember(&self->alloced_sigs, signo)) {
            sigaddset(&self->alloced_sigs, signo);
            r = signo;
        }
        else {
            errno = EBUSY;
        }
    }
    mtx_unlock(&self->mutex);

    return r;
}

void dispatch_free_signal(dispatch_t _Nonnull self, int signo)
{
    mtx_lock(&self->mutex);
    if (signo != 0 && signo != SIGKILL && signo != SIGDISP) {
        sigdelset(&self->alloced_sigs, signo);
    }
    mtx_unlock(&self->mutex);
}

vcpuid_t dispatch_signal_target(dispatch_t _Nonnull self)
{
    mtx_lock(&self->mutex);
    const id_t id = self->groupid;
    mtx_unlock(&self->mutex);

    return id;
}

int dispatch_send_signal(dispatch_t _Nonnull self, int signo)
{
    vcpuid_t id;
    int r, scope;

    if (signo == SIGDISP || signo == SIGKILL) {
        errno = EINVAL;
        return -1;
    }

    mtx_lock(&self->mutex);
    if (self->attr.maxConcurrency == 1 && self->workers.first) {
        id = ((dispatch_worker_t)self->workers.first)->id;
        scope = SIG_SCOPE_VCPU;
    }
    else {
        id = self->groupid;
        scope = SIG_SCOPE_VCPU_GROUP;
    }

    r = sigsend(scope, id, signo);
    mtx_unlock(&self->mutex);
    return r;
}
