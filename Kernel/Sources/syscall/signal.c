//
//  signal.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/5/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "syscalldecls.h"
#include <dispatcher/WaitQueue.h>
#include <kern/timespec.h>
#include <kpi/signal.h>


SYSCALL_2(sigwait, const sigset_t* _Nonnull set, siginfo_t* _Nullable info)
{
    decl_try_err();
    ProcessRef pp = (ProcessRef)p;

    const int sps = preempt_disable();
    err = WaitQueue_SigWait(&pp->siwaQueue, pa->set, pa->info);
    preempt_restore(sps);
    return err;
}

SYSCALL_4(sigtimedwait, const sigset_t* _Nonnull set, int flags, const struct timespec* _Nonnull wtp, siginfo_t* _Nullable info)
{
    decl_try_err();
    ProcessRef pp = (ProcessRef)p;

    const int sps = preempt_disable();
    err = WaitQueue_SigTimedWait(&pp->siwaQueue, pa->set, pa->flags, pa->wtp, pa->info);
    preempt_restore(sps);
    return err;
}

SYSCALL_1(sigpending, sigset_t* _Nonnull set)
{
    decl_try_err();
    VirtualProcessor* vp = (VirtualProcessor*)p;

    const int sps = preempt_disable();
    *(pa->set) = vp->psigs & ~vp->sigmask;
    preempt_restore(sps);
    return EOK;
}
