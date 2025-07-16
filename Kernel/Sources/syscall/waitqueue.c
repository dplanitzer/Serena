//
//  waitqueue.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/5/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "syscalldecls.h"
#include <dispatcher/WaitQueue.h>
#include <kern/timespec.h>
#include <kpi/signal.h>
#include <kpi/waitqueue.h>
#include <process/UWaitQueue.h>


SYSCALL_2(wq_create, int policy, int* _Nonnull pOutOd)
{
    decl_try_err();
    ProcessRef pp = vp->proc;
    UWaitQueueRef pq = NULL;

    switch (pa->policy) {
        case WAITQUEUE_FIFO:
            break;

        default:
            *(pa->pOutOd) = -1;
            return EINVAL;
    }

    try(UResource_AbstractCreate(&kUWaitQueueClass, (UResourceRef*)&pq));
    WaitQueue_Init(&pq->wq);
    pq->policy = pa->policy;

    try(UResourceTable_AdoptResource(&pp->uResourcesTable, (UResourceRef) pq, pa->pOutOd));
    return EOK;

catch:
    UResource_Dispose(pq);
    *(pa->pOutOd) = -1;
    return err;
}

SYSCALL_1(wq_dispose, int q)
{
    ProcessRef pp = vp->proc;

    return UResourceTable_DisposeResource(&pp->uResourcesTable, pa->q);
}

SYSCALL_2(wq_wait, int q, const sigset_t* _Nullable mask)
{
    decl_try_err();
    const sigset_t newMask = (pa->mask) ? *(pa->mask) & ~SIGSET_NONMASKABLES : 0;
    const sigset_t* pmask = (pa->mask) ? &newMask : NULL;
    ProcessRef pp = vp->proc;
    UWaitQueueRef pq;

    if ((err = UResourceTable_AcquireResourceAs(&pp->uResourcesTable, pa->q, UWaitQueue, &pq)) == EOK) {
        const int sps = preempt_disable();
        err = WaitQueue_Wait(&pq->wq, pmask);
        preempt_restore(sps);
        UResourceTable_RelinquishResource(&pp->uResourcesTable, pq);
    }
    return err;
}

SYSCALL_4(wq_timedwait, int q, const sigset_t* _Nullable mask, int flags, const struct timespec* _Nonnull wtp)
{
    decl_try_err();
    const sigset_t newMask = (pa->mask) ? *(pa->mask) & ~SIGSET_NONMASKABLES : 0;
    const sigset_t* pmask = (pa->mask) ? &newMask : NULL;
    ProcessRef pp = vp->proc;
    UWaitQueueRef pq;

    if ((err = UResourceTable_AcquireResourceAs(&pp->uResourcesTable, pa->q, UWaitQueue, &pq)) == EOK) {
        const int sps = preempt_disable();
        err = WaitQueue_TimedWait(&pq->wq, pmask, pa->flags, pa->wtp, NULL);
        preempt_restore(sps);
        UResourceTable_RelinquishResource(&pp->uResourcesTable, pq);
    }
    return err;
}

SYSCALL_5(wq_timedwakewait, int q, int oq, const sigset_t* _Nullable mask, int flags, const struct timespec* _Nonnull wtp)
{
    decl_try_err();
    const sigset_t newMask = (pa->mask) ? *(pa->mask) & ~SIGSET_NONMASKABLES : 0;
    const sigset_t* pmask = (pa->mask) ? &newMask : NULL;
    ProcessRef pp = vp->proc;
    UWaitQueueRef pq;
    UWaitQueueRef opq;

    if ((err = UResourceTable_AcquireTwoResourcesAs(&pp->uResourcesTable, pa->q, UWaitQueue, &pq, pa->oq, UWaitQueue, &opq)) == EOK) {
        const int sps = preempt_disable();
        WaitQueue_Wakeup(&opq->wq, WAKEUP_ONE | WAKEUP_CSW, WRES_WAKEUP);
        err = WaitQueue_TimedWait(&pq->wq, pmask, pa->flags, pa->wtp, NULL);
        preempt_restore(sps);
        UResourceTable_RelinquishTwoResources(&pp->uResourcesTable, pq, opq);
    }
    return err;
}

SYSCALL_2(wq_wakeup, int q, int flags)
{
    decl_try_err();
    const int wflags = ((pa->flags & WAKE_ONE) == WAKE_ONE) ? WAKEUP_ONE : WAKEUP_ALL;
    ProcessRef pp = vp->proc;
    UWaitQueueRef pq;

    if ((err = UResourceTable_BeginDirectResourceAccessAs(&pp->uResourcesTable, pa->q, UWaitQueue, &pq)) == EOK) {
        const int sps = preempt_disable();
        WaitQueue_Wakeup(&pq->wq, wflags | WAKEUP_CSW, WRES_WAKEUP);
        preempt_restore(sps);
        UResourceTable_EndDirectResourceAccess(&pp->uResourcesTable);
    }
    return err;
}
