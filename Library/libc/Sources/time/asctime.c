//
//  asctime.c
//  libc
//
//  Created by Dietmar Planitzer on 2/27/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include <time.h>
#include <stdio.h>
#include "gregorian_calendar.h"


char *asctime(const struct tm *timeptr)
{
    static char __gAscTimeBuffer[26];

    sprintf(__gAscTimeBuffer, "%s %s %3d %.2d:%.2d:%.2d %d\n",
                __gc_abbrev_wday(timeptr->tm_wday % 7),
                __gc_abbrev_ymon((timeptr->tm_mon % 12) + 1),
                timeptr->tm_mday,
                timeptr->tm_hour,
                timeptr->tm_min,
                timeptr->tm_sec,
                1900 + timeptr->tm_year);
    return __gAscTimeBuffer;
}
