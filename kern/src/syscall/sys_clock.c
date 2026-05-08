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


SYSCALL_4(clock_sleep, int clockid, int flags, const nanotime_t* _Nonnull wtp, nanotime_t* _Nullable rmtp)
{
    ProcessRef pp = vp->proc;
    int options = 0;
    const sigset_t sigs = sig_bit(SIG_TERMINATE);
    int signo;

    if (!nanotime_isvalid(pa->wtp)) {
        return EINVAL;
    }
    if (pa->clockid != CLOCK_MONOTONIC) {
        return ENODEV;
    }

    if ((pa->flags & TIMER_ABSTIME) == TIMER_ABSTIME) {
        options |= WAIT_ABSTIME;
    }


    const int sps = preempt_disable();
    nanotime_t start_t, stop_t;

    if (pa->rmtp) {
        clock_gettime(g_mono_clock, &start_t);
    }

    const bool timedout = vcpu_sigtimedwait(&pp->clk_wait_queue, &sigs, options, pa->wtp, &signo);

    if (pa->rmtp) {
        if (timedout) {
            // reached the end time
            nanotime_set(pa->rmtp, &NANOTIME_ZERO);
        }
        else {
            // got interrupted by a canceling signal
            clock_gettime(g_mono_clock, &stop_t);
            nanotime_sub(&stop_t, &start_t, pa->rmtp);
        }
    }

    preempt_restore(sps);
    
    return EOK;
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
