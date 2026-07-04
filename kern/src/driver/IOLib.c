//
//  IOLib.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/4/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "IOLib.h"
#include "IORegistry.h"
#include <sched/sched.h>
#include <ext/nanotime.h>
#include <hal/clock.h>
#include <sched/waitqueue.h>
#include <kern/devfs.h>
#ifdef MACHINE_AMIGA
#include <driver/hw/m68k-amiga/AmiExpert.h>
#else
#error "unknown platform"
#endif

static struct waitqueue g_sleep_wq; // VPs which block in a delay_xx() call wait on this wait queue


errno_t IOInit(void)
{
    decl_try_err();
    IODriverRef dp;
    Class* pcls;

    wq_init(&g_sleep_wq);


    // Create devfs and the I/O registry
    try(devfs_init());
    IORegistry_Init();


#ifdef MACHINE_AMIGA
    pcls = class(AmiExpert);
#else
    abort();
#endif

    // Platform expert
    try(IOPlatformExpert_Create(pcls, &dp));
    try(IODriver_Launch(dp, NULL));
    gIOPlatformExpert = (IOPlatformExpertRef)dp;

catch:
    return err;
}

void IODelay(useconds_t us)
{
    nanotime_t ts, now, abs_deadline;

    nanotime_from_us(&ts, us);
    clock_gettime_hires(g_mono_clock, &now);
    nanotime_add(&abs_deadline, &now, &ts);

    while (nanotime_lt(&now, &abs_deadline)) {
        clock_gettime_hires(g_mono_clock, &now);
    }
}

void IOSleep(mseconds_t ms)
{
    nanotime_t ts;

    nanotime_from_ms(&ts, ms);
    const ticks_t deadline = wq_calc_deadline(g_mono_clock, 0, &ts);
    const int sps = preempt_disable();
    wq_wait_np(&g_sleep_wq, deadline);
    preempt_restore(sps);
}

