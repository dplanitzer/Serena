//
//  mktime.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <time.h>
#include <errno.h>
#include "gregorian_calendar.h"


time_t mktime(struct tm *timeptr)
{
    //XXX clamping isn't quite right
    if (timeptr->tm_year < 0) {
        timeptr->tm_year = 0;
    }
    if (timeptr->tm_mon < 0) {
        timeptr->tm_mon = 0;
    }
    if (timeptr->tm_mon > 11) {
        timeptr->tm_mon = 11;
    }
    if (timeptr->tm_mday < 0) {
        timeptr->tm_mday = 0;
    }
    if (timeptr->tm_mday > 31) {
        timeptr->tm_mday = 31;
    }
    if (timeptr->tm_hour < 0) {
        timeptr->tm_hour = 0;
    }
    if (timeptr->tm_hour > 23) {
        timeptr->tm_hour = 23;
    }
    if (timeptr->tm_min < 0) {
        timeptr->tm_min = 0;
    }
    if (timeptr->tm_min > 59) {
        timeptr->tm_min = 59;
    }
    if (timeptr->tm_sec < 0) {
        timeptr->tm_sec = 0;
    }
    if (timeptr->tm_sec > 60) {
        timeptr->tm_sec = 60;
    }

    const int year = timeptr->tm_year + 1900;
    const days_t days = __gc_days_from_civil(year, (unsigned)(timeptr->tm_mon + 1), (unsigned)timeptr->tm_mday);
    const time_t secs = timeptr->tm_hour * 60 * 60 + timeptr->tm_min * 60 + timeptr->tm_sec;
    const time_t r = (time_t)(days * 86400) + secs;

    if (r >= 0) {
        timeptr->tm_wday = __gc_weekday_from_days(days);
        timeptr->tm_yday = days - __gc_days_from_civil(year, 1, 1);
        return r;
    }
    else {
        errno = EOVERFLOW;
        return (time_t)-1;
    }
}
