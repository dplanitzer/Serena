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


SYSCALL_2(wq_create, int policy, int* _Nonnull pOutOd)
{
    return Process_CreateUWaitQueue((ProcessRef)p, pa->policy, pa->pOutOd);
}

SYSCALL_2(wq_wait, int q, const sigset_t* _Nullable mask)
{
    ProcessRef pp = (ProcessRef)p;
    const sigset_t newMask = (pa->mask) ? *(pa->mask) & ~SIGSET_NONMASKABLES : 0;
    const sigset_t* pmask = (pa->mask) ? &newMask : NULL;

    return Process_Wait_UWaitQueue(pp, pa->q, pa->mask);
}

SYSCALL_4(wq_timedwait, int q, const sigset_t* _Nullable mask, int flags, const struct timespec* _Nonnull wtp)
{
    ProcessRef pp = (ProcessRef)p;
    const sigset_t newMask = (pa->mask) ? *(pa->mask) & ~SIGSET_NONMASKABLES : 0;
    const sigset_t* pmask = (pa->mask) ? &newMask : NULL;

    return Process_TimedWait_UWaitQueue(pp, pa->q, pa->mask, pa->flags, pa->wtp);
}

SYSCALL_5(wq_timedwakewait, int q, int oq, const sigset_t* _Nullable mask, int flags, const struct timespec* _Nonnull wtp)
{
    ProcessRef pp = (ProcessRef)p;
    const sigset_t newMask = (pa->mask) ? *(pa->mask) & ~SIGSET_NONMASKABLES : 0;
    const sigset_t* pmask = (pa->mask) ? &newMask : NULL;

    return Process_TimedWakeWait_UWaitQueue(pp, pa->q, pa->oq, pa->mask, pa->flags, pa->wtp);
}

SYSCALL_2(wq_wakeup, int q, int flags)
{
    return Process_Wakeup_UWaitQueue((ProcessRef)p, pa->q, pa->flags);
}
