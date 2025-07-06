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


SYSCALL_2(sigwait, const sigset_t* _Nullable mask, const sigset_t* _Nonnull set)
{
#if 0
    ProcessRef pp = (ProcessRef)p;
    const sigset_t newMask = (pa->mask) ? *(pa->mask) & ~SIGSET_NONMASKABLES : 0;
    const sigset_t* pmask = (pa->mask) ? &newMask : NULL;

    return Process_SigWait_UWaitQueue((ProcessRef)p, pa->od, pmask, pa->osigs);
#else
    return 0;
#endif
}

SYSCALL_4(sigtimedwait, const sigset_t* _Nullable mask, const sigset_t* _Nonnull set, int flags, const struct timespec* _Nonnull wtp)
{
#if 0
    const sigset_t newMask = (pa->mask) ? *(pa->mask) & ~SIGSET_NONMASKABLES : 0;
    const sigset_t* pmask = (pa->mask) ? &newMask : NULL;

    return Process_SigTimedWait_UWaitQueue((ProcessRef)p, pa->od, pmask, pa->osigs, pa->flags, pa->wtp);
#else
    return 0;
#endif
}
