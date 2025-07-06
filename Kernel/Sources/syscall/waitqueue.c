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
#include <process/UWaitQueue.h>


SYSCALL_2(wq_create, int policy, int* _Nonnull pOutOd)
{
    decl_try_err();
    ProcessRef pp = (ProcessRef)p;
    UWaitQueueRef pq = NULL;

    try(UWaitQueue_Create(pa->policy, &pq));
    try(UResourceTable_AdoptResource(&pp->uResourcesTable, (UResourceRef) pq, pa->pOutOd));
    return EOK;

catch:
    UResource_Dispose(pq);
    *(pa->pOutOd) = -1;
    return err;
}

SYSCALL_2(wq_wait, int q, const sigset_t* _Nullable mask)
{
    decl_try_err();
    const sigset_t newMask = (pa->mask) ? *(pa->mask) & ~SIGSET_NONMASKABLES : 0;
    const sigset_t* pmask = (pa->mask) ? &newMask : NULL;
    ProcessRef pp = (ProcessRef)p;
    UWaitQueueRef pq;

    if ((err = UResourceTable_AcquireResourceAs(&pp->uResourcesTable, pa->q, UWaitQueue, &pq)) == EOK) {
        err = UWaitQueue_Wait(pq, pa->mask);
        UResourceTable_RelinquishResource(&pp->uResourcesTable, pq);
    }
    return err;
}

SYSCALL_4(wq_timedwait, int q, const sigset_t* _Nullable mask, int flags, const struct timespec* _Nonnull wtp)
{
    decl_try_err();
    const sigset_t newMask = (pa->mask) ? *(pa->mask) & ~SIGSET_NONMASKABLES : 0;
    const sigset_t* pmask = (pa->mask) ? &newMask : NULL;
    ProcessRef pp = (ProcessRef)p;
    UWaitQueueRef pq;

    if ((err = UResourceTable_AcquireResourceAs(&pp->uResourcesTable, pa->q, UWaitQueue, &pq)) == EOK) {
        err = UWaitQueue_TimedWait(pq, pa->mask, pa->flags, pa->wtp);
        UResourceTable_RelinquishResource(&pp->uResourcesTable, pq);
    }
    return err;
}

SYSCALL_5(wq_timedwakewait, int q, int oq, const sigset_t* _Nullable mask, int flags, const struct timespec* _Nonnull wtp)
{
    decl_try_err();
    const sigset_t newMask = (pa->mask) ? *(pa->mask) & ~SIGSET_NONMASKABLES : 0;
    const sigset_t* pmask = (pa->mask) ? &newMask : NULL;
    ProcessRef pp = (ProcessRef)p;
    UWaitQueueRef pq;
    UWaitQueueRef opq;

    if ((err = UResourceTable_AcquireTwoResourcesAs(&pp->uResourcesTable, pa->q, UWaitQueue, &pq, pa->oq, UWaitQueue, &opq)) == EOK) {
        err = UWaitQueue_TimedWakeWait(pq, opq, pa->mask, pa->flags, pa->wtp);
        UResourceTable_RelinquishTwoResources(&pp->uResourcesTable, pq, opq);
    }
    return err;
}

SYSCALL_2(wq_wakeup, int q, int flags)
{
    decl_try_err();
    ProcessRef pp = (ProcessRef)p;
    UWaitQueueRef pq;

    if ((err = UResourceTable_BeginDirectResourceAccessAs(&pp->uResourcesTable, pa->q, UWaitQueue, &pq)) == EOK) {
        UWaitQueue_Wakeup(pq, pa->flags);
        UResourceTable_EndDirectResourceAccess(&pp->uResourcesTable);
    }
    return err;
}
