//
//  waitqueue.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/5/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "syscalldecls.h"
#include <kern/timespec.h>
#include <klib/Hash.h>
#include <kern/kalloc.h>
#include <klib/List.h>
#include <kpi/signal.h>
#include <machine/csw.h>
#include <sched/waitqueue.h>


static errno_t uwq_create(int policy, UWaitQueue* _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    UWaitQueue* self = NULL;

    if (policy != WAITQUEUE_FIFO) {
        *pOutSelf = NULL;
        return EINVAL;
    }

    err = kalloc(sizeof(UWaitQueue), (void**)&self);
    if (err == EOK) {
        ListNode_Init(&self->qe);
        wq_init(&self->wq);
        self->policy = policy;
        self->id = -1;
    }

    *pOutSelf = self;
    return err;
}

void uwq_destroy(UWaitQueue* _Nullable self)
{
    if (self) {
        wq_deinit(&self->wq);
        kfree(self);
    }
}


SYSCALL_2(wq_create, int policy, int* _Nonnull pOutQ)
{
    decl_try_err();
    ProcessRef pp = vp->proc;
    UWaitQueue* uwp = NULL;
    int q = -1;

    err = uwq_create(pa->policy, &uwp);
    if (err == EOK) {
        const int sps = preempt_disable();
        
        q = pp->nextAvailWaitQueueId++;
        uwp->id = q;
        List_InsertAfterLast(&pp->waitQueueTable[hash_scalar(q) & UWQ_HASH_CHAIN_MASK], &uwp->qe);
        preempt_restore(sps);
    }
    *(pa->pOutQ) = q;

    return err;
}

// @Entry Condition: preemption disabled
static UWaitQueue* _Nullable _find_uwq(ProcessRef _Nonnull pp, int q)
{
    List_ForEach(&pp->waitQueueTable[hash_scalar(q) & UWQ_HASH_CHAIN_MASK], ListNode,
        UWaitQueue* cwp = (UWaitQueue*)pCurNode;

        if (cwp->id == q) {
            return cwp;
        }
    );

    return NULL;
}

SYSCALL_1(wq_dispose, int q)
{
    decl_try_err();
    ProcessRef pp = vp->proc;

    const int sps = preempt_disable();
    UWaitQueue* uwp = _find_uwq(pp, pa->q);
    
    if (uwp) {
        if (List_IsEmpty(&uwp->wq.q)) {
            List_Remove(&pp->waitQueueTable[hash_scalar(pa->q) & UWQ_HASH_CHAIN_MASK], &uwp->qe);
        }
        else {
            err = EBUSY;
        }
    }
    else {
        err = EBADF;
    }
    preempt_restore(sps);
    
    if (err == EOK && uwp) {
        uwq_destroy(uwp);
    }

    return err;
}

SYSCALL_2(wq_wait, int q, const sigset_t* _Nullable set)
{
    decl_try_err();
    const sigset_t r_set = (pa->set) ? *(pa->set) | SIGSET_NONMASKABLES : SIGSET_NONMASKABLES;
    ProcessRef pp = vp->proc;

    const int sps = preempt_disable();
    UWaitQueue* uwp = _find_uwq(pp, pa->q);

    err = (uwp) ? wq_wait(&uwp->wq, &r_set) : EBADF;
    preempt_restore(sps);
    return err;
}

SYSCALL_4(wq_timedwait, int q, const sigset_t* _Nullable set, int flags, const struct timespec* _Nonnull wtp)
{
    decl_try_err();
    const sigset_t r_set = (pa->set) ? *(pa->set) | SIGSET_NONMASKABLES : SIGSET_NONMASKABLES;
    ProcessRef pp = vp->proc;

    const int sps = preempt_disable();
    UWaitQueue* uwp = _find_uwq(pp, pa->q);

    err = (uwp) ? wq_timedwait(&uwp->wq, &r_set, pa->flags, pa->wtp, NULL) : EBADF;
    preempt_restore(sps);
    return err;
}

SYSCALL_5(wq_timedwakewait, int q, int oq, const sigset_t* _Nullable set, int flags, const struct timespec* _Nonnull wtp)
{
    decl_try_err();
    const sigset_t r_set = (pa->set) ? *(pa->set) | SIGSET_NONMASKABLES : SIGSET_NONMASKABLES;
    ProcessRef pp = vp->proc;

    const int sps = preempt_disable();
    UWaitQueue* uwp = _find_uwq(pp, pa->q);
    UWaitQueue* owp = _find_uwq(pp, pa->oq);

    if (uwp && owp) {
        wq_wake(&owp->wq, WAKEUP_ONE | WAKEUP_CSW, WRES_WAKEUP);
        err = wq_timedwait(&uwp->wq, &r_set, pa->flags, pa->wtp, NULL);
    }
    else {
        err = EBADF;
    }
    preempt_restore(sps);
    return err;
}

SYSCALL_2(wq_wakeup, int q, int flags)
{
    decl_try_err();
    const int wflags = ((pa->flags & WAKE_ONE) == WAKE_ONE) ? WAKEUP_ONE : WAKEUP_ALL;
    ProcessRef pp = vp->proc;

    const int sps = preempt_disable();
    UWaitQueue* uwp = _find_uwq(pp, pa->q);

    if (uwp) {
        wq_wake(&uwp->wq, wflags | WAKEUP_CSW, WRES_WAKEUP);
    }
    else {
        err = EBADF;
    }
    preempt_restore(sps);
    return err;
}
