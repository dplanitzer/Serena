//
//  sys_clock.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/5/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "syscalldecls.h"
#include <ext/nanotime.h>
#include <hal/clock.h>
#include <hal/sched.h>
#include <kpi/clock.h>


SYSCALL_4(clock_wait, int clockid, int flags, const nanotime_t* _Nonnull wtp, nanotime_t* _Nullable rmtp)
{
    if (!nanotime_isvalid(pa->wtp)) {
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
    const int err = wq_timedwait(&pp->clk_wait_queue, NULL, options, pa->wtp, pa->rmtp);
    preempt_restore(sps);
    
    return (err != ETIMEDOUT) ? err : EOK;
}

SYSCALL_2(clock_time, int clockid, nanotime_t* _Nonnull time)
{
    if (pa->clockid != CLOCK_MONOTONIC) {
        return ENODEV;
    }

    clock_gettime(g_mono_clock, pa->time);
    return EOK;
}

SYSCALL_3(clock_info, int clockid, int flavor, clock_info_ref _Nonnull info)
{
    if (pa->clockid != CLOCK_MONOTONIC) {
        return ENODEV;
    }

    switch (pa->flavor) {
        case CLOCK_INFO_BASIC: {
            clock_basic_info_t* ip = pa->info;

            clock_getresolution(g_mono_clock, &ip->tick_resolution);
            break;
        }

        default:
            return EINVAL;
    }

    return EOK;
}
