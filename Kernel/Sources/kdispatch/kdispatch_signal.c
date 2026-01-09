//
//  kdispatch_signal.c
//  kernel
//
//  Created by Dietmar Planitzer on 9/21/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "kdispatch_priv.h"


static void _kdispatch_enable_signal(kdispatch_t _Nonnull _Locked self, int signo, bool enable)
{
    deque_for_each(&self->workers, deque_node_t, {
        kdispatch_worker_t cwp = (kdispatch_worker_t)pCurNode;

        if (enable) {
            cwp->hotsigs |= _SIGBIT(signo);
        }
        else {
            cwp->hotsigs &= ~_SIGBIT(signo);
        }
    });


    // Notify all workers so that they can pick up the new hotsigs set.
    _kdispatch_wakeup_all_workers(self);
}

// Removes the signal monitor 'item' from its signal trap and retires it.
void _kdispatch_withdraw_signal_item(kdispatch_t _Nonnull self, kdispatch_item_t _Nonnull item)
{
    const int signo = item->subtype;
    kdispatch_sigtrap_t stp = NULL;
    kdispatch_item_t pip = NULL;
    bool hasIt = false;

    if (self->sigtraps) {
        stp = &self->sigtraps[signo - 1];

        SList_ForEach(&stp->monitors, SListNode, {
            kdispatch_item_t cip = (kdispatch_item_t)pCurNode;

            if (cip == item) {
                SList_Remove(&stp->monitors, &pip->qe, &cip->qe);
                hasIt = true;
                break;
            }

            pip = cip;
        });
    }

    if (hasIt) {
        _kdispatch_retire_item(self, item);

        stp->count--;
        if (stp->count == 0) {
            _kdispatch_enable_signal(self, signo, false);
        }
    }
}

// Retires the signal monitor 'item'.
void _kdispatch_retire_signal_item(kdispatch_t _Nonnull self, kdispatch_item_t _Nonnull item)
{
    const int signo = item->subtype;

    _kdispatch_retire_item(self, item);

    if (self->sigtraps) {
        kdispatch_sigtrap_t stp = &self->sigtraps[signo - 1];

        stp->count--;
        if (stp->count == 0) {
            _kdispatch_enable_signal(self, signo, false);
        }
    }
}

// Rearms 'item' in the sense that it is moved back to idle state and the signal
// trap so that it can be submitted again when the next signal comes in.
bool _kdispatch_rearm_signal_item(kdispatch_t _Nonnull _Locked self, kdispatch_item_t _Nonnull item)
{
    if (self->sigtraps) {
        kdispatch_sigtrap_t stp = &self->sigtraps[item->subtype - 1];

        item->state = KDISPATCH_STATE_IDLE;
        item->qe = SLISTNODE_INIT;

        SList_InsertAfterLast(&stp->monitors, &item->qe);
        return true;
    }
    else {
        return false;
    }
}

static errno_t _kdispatch_item_on_signal(kdispatch_t _Nonnull _Locked self, int signo, kdispatch_item_t _Nonnull item)
{
    decl_try_err();

    // Ensure that there's at least one worker alive
    if ((err = _kdispatch_ensure_worker_capacity(self, _KDISPATCH_EWC_TIMER)) != EOK) {
        return err;
    }


    if (self->sigtraps == NULL) {
        //XXX allocate in a smarter way: eg organize sigset in quarters, calc
        //XXX what's the highest quarter we need and only allocate up to this
        //XXX quarter.
        if ((err = kalloc_cleared(SIGMAX * sizeof(struct kdispatch_sigtrap), (void**)&self->sigtraps)) != EOK) {
            return err;
        }
    }

    item->qe = SLISTNODE_INIT;
    item->type = _KDISPATCH_TYPE_USER_SIGNAL_ITEM;
    item->subtype = (uint8_t)signo;
    item->flags = _KDISPATCH_ITEM_FLAG_REPEATING;
    item->state = KDISPATCH_STATE_IDLE;

    kdispatch_sigtrap_t stp = &self->sigtraps[signo - 1];
    SList_InsertAfterLast(&stp->monitors, &item->qe);
    stp->count++;

    if (stp->count == 1) {
        _kdispatch_enable_signal(self, signo, true);
    }

    return EOK;
}

void _kdispatch_submit_items_for_signal(kdispatch_t _Nonnull _Locked self, int signo, kdispatch_worker_t _Nonnull worker)
{
    if (self->sigtraps) {
        kdispatch_sigtrap_t stp = &self->sigtraps[signo - 1];

        while (stp->monitors.first) {
            kdispatch_item_t item = (kdispatch_item_t)SList_RemoveFirst(&stp->monitors);

            item->qe = SLISTNODE_INIT;
            item->state = KDISPATCH_STATE_SCHEDULED;
            item->flags &= ~_KDISPATCH_ITEM_FLAG_CANCELLED;

            // No need to wakeup ourselves. This function is called from the worker
            // 'worker' and we know we're already awake.
            _kdispatch_worker_submit(worker, item, false);
        }
    }
}


////////////////////////////////////////////////////////////////////////////////
// MARK: API

static sigset_t _SIGSET_NOSENDMON = _SIGBIT(SIGDISP) | _SIGBIT(SIGKILL) | _SIGBIT(SIGVPRQ) | _SIGBIT(SIGVPDS) | _SIGBIT(SIGSTOP);

errno_t kdispatch_item_on_signal(kdispatch_t _Nonnull self, int signo, kdispatch_item_t _Nonnull item)
{
    decl_try_err();

    if (signo < SIGMIN || signo > SIGMAX || (_SIGSET_NOSENDMON & _SIGBIT(signo)) != 0) {
        return EINVAL;
    }

    mtx_lock(&self->mutex);
    if (self->state < _DISPATCHER_STATE_TERMINATING) {
        err = _kdispatch_item_on_signal(self, signo, item);
    }
    else {
        err = ETERMINATED;
    }
    mtx_unlock(&self->mutex);
    return err;
}

int kdispatch_alloc_signal(kdispatch_t _Nonnull self, int signo)
{
    int r = -1;

    mtx_lock(&self->mutex);
    if (signo <= 0) {
        // Allocate the first lowest priority USR signal available
        for (int i = SIGUSRMAX; i >= SIGUSRMIN; i--) {
            sigset_t sigbit = _SIGBIT(i);

            if ((self->alloced_sigs & sigbit) == 0) {
                self->alloced_sigs |= sigbit;
                r = i;
                break;
            }
        }
    }
    else if (signo >= SIGUSRMIN && signo <= SIGUSRMAX) {
        // Allocate the specific USR signal 'signo'
        const sigset_t sigbit = _SIGBIT(signo);

        if ((self->alloced_sigs & sigbit) == 0) {
            self->alloced_sigs |= sigbit;
            r = signo;
        }
    }
    mtx_unlock(&self->mutex);

    return r;
}

void kdispatch_free_signal(kdispatch_t _Nonnull self, int signo)
{
    mtx_lock(&self->mutex);
    if (signo >= SIGUSRMIN && signo <= SIGUSRMAX) {
        self->alloced_sigs &= ~_SIGBIT(signo);
    }
    mtx_unlock(&self->mutex);
}

vcpuid_t kdispatch_signal_target(kdispatch_t _Nonnull self)
{
    mtx_lock(&self->mutex);
    const id_t id = self->groupid;
    mtx_unlock(&self->mutex);

    return id;
}

errno_t kdispatch_send_signal(kdispatch_t _Nonnull self, int signo)
{
    decl_try_err();
    vcpuid_t id;
    int scope;

    if (signo < SIGMIN || signo > SIGMAX || (_SIGSET_NOSENDMON & _SIGBIT(signo)) != 0) {
        return EINVAL;
    }

    mtx_lock(&self->mutex);
    if (self->attr.maxConcurrency == 1 && self->workers.first) {
        vcpu_sigsend(((kdispatch_worker_t)self->workers.first)->vcpu, signo);
    }
    else {
        deque_for_each(&self->workers, deque_node_t, {
            kdispatch_worker_t cwp = (kdispatch_worker_t)pCurNode;

            vcpu_sigsend(cwp->vcpu, signo);
        });
    }

    mtx_unlock(&self->mutex);
    return EOK;
}
