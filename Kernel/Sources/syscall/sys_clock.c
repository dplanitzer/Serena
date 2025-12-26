//
//  sys_clock.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/5/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "syscalldecls.h"
#include <ext/timespec.h>
#include <hal/clock.h>
#include <hal/sched.h>


SYSCALL_4(clock_nanosleep, int clockid, int flags, const struct timespec* _Nonnull wtp, struct timespec* _Nullable rmtp)
{
    if (!timespec_isvalid(pa->wtp)) {
        return EINVAL;
    }
    if (pa->clockid != CLOCK_MONOTONIC) {
        return ENODEV;
    }


    int options = 0;
    if ((pa->flags & TIMER_ABSTIME) == TIMER_ABSTIME) {
        options |= WAIT_ABSTIME;
    }


    // This is a medium or long wait -> context switch away
    ProcessRef pp = vp->proc;
    const int sps = preempt_disable();
    const int err = wq_timedwait(&pp->sleep_queue, NULL, options, pa->wtp, pa->rmtp);
    preempt_restore(sps);
    
    return (err != ETIMEDOUT) ? err : EOK;
}

SYSCALL_2(clock_gettime, int clockid, struct timespec* _Nonnull time)
{
    if (pa->clockid != CLOCK_MONOTONIC) {
        return ENODEV;
    }

    clock_gettime(g_mono_clock, pa->time);
    return EOK;
}

SYSCALL_2(clock_getres, int clockid, struct timespec* _Nonnull res)
{
    if (pa->clockid != CLOCK_MONOTONIC) {
        return ENODEV;
    }

    clock_getresolution(g_mono_clock, pa->res);
    return EOK;
}
