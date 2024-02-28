//
//  time.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <time.h>
#include <System/Clock.h>


clock_t clock(void)
{
    const TimeInterval ti = MonotonicClock_GetTime();

    return ti.tv_sec * CLOCKS_PER_SEC + ti.tv_nsec / ((1000l * 1000l * 1000l) / CLOCKS_PER_SEC);
}

// XXX implement me
time_t mktime(struct tm *timeptr)
{
    return -1;
}

// XXX implement me
time_t time(time_t *timer)
{
    return -1;
}

// XXX implement me
struct tm *gmtime(const time_t *timer)
{
    return NULL;
}

// XXX implement me
struct tm *localtime(const time_t *timer)
{
    return NULL;
}

double difftime(time_t time1, time_t time0)
{
    return (double)time1 - (double)time0;
}
