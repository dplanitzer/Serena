//
//  delay.c
//  kernel
//
//  Created by Dietmar Planitzer on 6/29/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "delay.h"
#include "WaitQueue.h"
#include <hal/MonotonicClock.h>
#include <hal/Platform.h>
#include <kern/timespec.h>

static WaitQueue   gSleepQueue;    // VPs which block in a delay_xx() call wait on this wait queue



void delay_init(void)
{
    WaitQueue_Init(&gSleepQueue);
}

void delay_us(useconds_t us)
{
    struct timespec wt;

    timespec_from_us(&wt, us);

    // Use the Delay() facility for short waits and context switching for medium and long waits
    if (MonotonicClock_Delay(false, &wt)) {
        return;
    }
    
    
    // This is a medium or long wait -> context switch away
    (void)_sleep(0, &wt, NULL);
}

void delay_ms(mseconds_t ms)
{
    delay_us(ms * 1000l);
}

void delay_sec(time_t sec)
{
    delay_us(sec * USEC_PER_SEC);
}

// Sleep for the given number of seconds.
errno_t _sleep(int flags, const struct timespec* _Nonnull wtp, struct timespec* _Nullable rmtp)
{
    const int sps = preempt_disable();
    const int err = WaitQueue_TimedWait(&gSleepQueue, flags, wtp, rmtp);
    preempt_restore(sps);
    
    return err;
}
