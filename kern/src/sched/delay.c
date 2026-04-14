//
//  delay.c
//  kernel
//
//  Created by Dietmar Planitzer on 6/29/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "delay.h"
#include "sched.h"
#include <ext/nanotime.h>
#include <hal/clock.h>
#include <sched/waitqueue.h>

// At most 1ms
#define DELAY_SPIN_MAX_NSEC    1000000l


static struct waitqueue gSleepQueue;    // VPs which block in a delay_xx() call wait on this wait queue


void delay_init(void)
{
    wq_init(&gSleepQueue);
}

static void _delay_by(const nanotime_t* _Nonnull wtp)
{
    // Just spin for very short waits and context switching for medium and long waits
    if (wtp->tv_sec == 0 && wtp->tv_nsec < DELAY_SPIN_MAX_NSEC) {
        nanotime_t now, deadline;
    
        clock_gettime_hires(g_mono_clock, &now);
        nanotime_add(&now, wtp, &deadline);

        // Just spin for now (would be nice to put the CPU to sleep though for a few micros before rechecking the time or so)
        while (nanotime_lt(&now, &deadline)) {
            clock_gettime_hires(g_mono_clock, &now);
        }
        return;
    }
    
    
    // This is a medium or long wait -> context switch away
    const int sps = preempt_disable();
    const int err = wq_timedwait(&gSleepQueue, NULL, 0, wtp, NULL);
    preempt_restore(sps);
}

void delay_us(useconds_t us)
{
    nanotime_t ts;

    nanotime_from_us(&ts, us);
    _delay_by(&ts);
}

void delay_ms(mseconds_t ms)
{
    nanotime_t ts;

    nanotime_from_ms(&ts, ms);
    _delay_by(&ts);
}

void delay_sec(time_t secs)
{
    nanotime_t ts;

    nanotime_from(&ts, secs, 0);
    _delay_by(&ts);
}
