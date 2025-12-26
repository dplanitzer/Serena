//
//  sys_waitqueue.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/5/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "syscalldecls.h"
#include <ext/hash.h>
#include <ext/timespec.h>
#include <hal/sched.h>
#include <kern/kalloc.h>
#include <kpi/signal.h>
#include <sched/waitqueue.h>


static errno_t uwq_create(int policy, u_wait_queue_t _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    u_wait_queue_t self = NULL;

    if (policy != WAITQUEUE_FIFO) {
        *pOutSelf = NULL;
        return EINVAL;
    }

    err = kalloc(sizeof(struct u_wait_queue), (void**)&self);
    if (err == EOK) {
        self->qe = LISTNODE_INIT;
        wq_init(&self->wq);
        self->policy = policy;
        self->id = -1;
    }

    *pOutSelf = self;
    return err;
}

void uwq_destroy(u_wait_queue_t _Nullable self)
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
    u_wait_queue_t uwp = NULL;
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
static u_wait_queue_t _Nullable _find_uwq(ProcessRef _Nonnull pp, int q)
{
    List_ForEach(&pp->waitQueueTable[hash_scalar(q) & UWQ_HASH_CHAIN_MASK], ListNode,
        u_wait_queue_t cwp = (u_wait_queue_t)pCurNode;

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
    u_wait_queue_t uwp = _find_uwq(pp, pa->q);
    
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

SYSCALL_1(wq_wait, int q)
{
    decl_try_err();
    ProcessRef pp = vp->proc;

    const int sps = preempt_disable();
    u_wait_queue_t uwp = _find_uwq(pp, pa->q);

    err = (uwp) ? wq_wait(&uwp->wq, NULL) : EBADF;
    preempt_restore(sps);
    return err;
}

SYSCALL_3(wq_timedwait, int q, int flags, const struct timespec* _Nonnull wtp)
{
    decl_try_err();
    ProcessRef pp = vp->proc;

    const int sps = preempt_disable();
    u_wait_queue_t uwp = _find_uwq(pp, pa->q);

    err = (uwp) ? wq_timedwait(&uwp->wq, NULL, pa->flags, pa->wtp, NULL) : EBADF;
    preempt_restore(sps);
    return err;
}

SYSCALL_4(wq_wakeup_then_timedwait, int q1, int q2, int flags, const struct timespec* _Nonnull wtp)
{
    decl_try_err();
    ProcessRef pp = vp->proc;

    const int sps = preempt_disable();
    u_wait_queue_t uwp_to_wake = _find_uwq(pp, pa->q1);
    u_wait_queue_t uwp_to_wait = _find_uwq(pp, pa->q2);

    if (uwp_to_wake && uwp_to_wait) {
        wq_wake(&uwp_to_wake->wq, WAKEUP_ONE | WAKEUP_CSW, WRES_WAKEUP);
        err = wq_timedwait(&uwp_to_wait->wq, NULL, pa->flags, pa->wtp, NULL);
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
    u_wait_queue_t uwp = _find_uwq(pp, pa->q);

    if (uwp) {
        wq_wake(&uwp->wq, wflags | WAKEUP_CSW, WRES_WAKEUP);
    }
    else {
        err = EBADF;
    }
    preempt_restore(sps);
    return err;
}
