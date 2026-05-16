//
//  sys_clock.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/5/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "syscalldecls.h"
#include <ext/nanotime.h>
#include <hal/sched.h>
#include <kern/sigset.h>
#include <kpi/clock.h>


SYSCALL_3(clock_sleep, int clockid, int flags, const nanotime_t* _Nonnull wtp)
{
    decl_try_err();
    ProcessRef pp = vp->proc;
    const sigset_t sigs = SIGSET_EMPTY;
    int signo;

    if (!nanotime_isvalid(pa->wtp)) {
        return EINVAL;
    }
    if ((pa->flags & ~TIMER_ABSTIME) != 0) {
        return EINVAL;
    }
    if (pa->clockid != CLOCK_MONOTONIC) {
        return ENODEV;
    }


    const ticks_t deadline = wq_calc_deadline(g_mono_clock, pa->flags, pa->wtp);
    const int sps = preempt_disable();
    err = vcpu_sigwait(&pp->clk_wait_queue, &sigs, 0, deadline, &signo);
    preempt_restore(sps);
    
    return err;
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
