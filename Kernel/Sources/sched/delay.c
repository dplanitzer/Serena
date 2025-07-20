//
//  delay.c
//  kernel
//
//  Created by Dietmar Planitzer on 6/29/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "delay.h"
#include <machine/csw.h>
#include <machine/MonotonicClock.h>
#include <kern/timespec.h>
#include <sched/waitqueue.h>

static struct waitqueue gSleepQueue;    // VPs which block in a delay_xx() call wait on this wait queue


void delay_init(void)
{
    wq_init(&gSleepQueue);
}

static void _delay_by(const struct timespec* _Nonnull wtp)
{
    // Use the Delay() facility for short waits and context switching for medium and long waits
    if (wtp->tv_sec == 0 && wtp->tv_nsec < MONOTONIC_DELAY_MAX_NSEC) {
        MonotonicClock_Delay(gMonotonicClock, wtp->tv_nsec);
        return;
    }
    
    
    // This is a medium or long wait -> context switch away
    const int sps = csw_disable();
    const int err = wq_timedwait(&gSleepQueue, NULL, 0, wtp, NULL);
    csw_restore(sps);
}

void delay_us(useconds_t us)
{
    struct timespec ts;

    timespec_from_us(&ts, us);
    _delay_by(&ts);
}

void delay_ms(mseconds_t ms)
{
    struct timespec ts;

    timespec_from_ms(&ts, ms);
    _delay_by(&ts);
}

void delay_sec(time_t secs)
{
    struct timespec ts;

    timespec_from(&ts, secs, 0);
    _delay_by(&ts);
}
