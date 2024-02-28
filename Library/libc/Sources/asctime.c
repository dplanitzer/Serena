//
//  asctime.c
//  libc
//
//  Created by Dietmar Planitzer on 2/27/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include <time.h>
#include <stdio.h>


static const char* __gDayOfWeek[7] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
static const char* __gMonthOfYear[12] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
static char __gAscTimeBuffer[26];

char *asctime(const struct tm *timeptr)
{
    sprintf(__gAscTimeBuffer, "%s %s %3d %.2d:%.2d:%.2d %d\n",
                __gDayOfWeek[timeptr->tm_wday],
                __gMonthOfYear[timeptr->tm_mon],
                timeptr->tm_mday,
                timeptr->tm_hour,
                timeptr->tm_min,
                timeptr->tm_sec,
                1900 + timeptr->tm_year);
    return __gAscTimeBuffer;
}

char *ctime(const time_t *timer)
{
    struct tm* lt = localtime(timer);

    return (lt) ? asctime(lt) : "";
}
