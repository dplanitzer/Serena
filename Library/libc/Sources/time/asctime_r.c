//
//  asctime.c
//  libc
//
//  Created by Dietmar Planitzer on 1/9/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#define _POSIX_SOURCE
#include <time.h>
#include <stdio.h>
#include "gregorian_calendar.h"


char *asctime_r(const struct tm *timeptr, char *buf)
{
    sprintf(buf, "%s %s %3d %.2d:%.2d:%.2d %d\n",
                __gc_abbrev_wday(timeptr->tm_wday % 7),
                __gc_abbrev_ymon((timeptr->tm_mon % 12) + 1),
                timeptr->tm_mday,
                timeptr->tm_hour,
                timeptr->tm_min,
                timeptr->tm_sec,
                1900 + timeptr->tm_year);
    return buf;
}
