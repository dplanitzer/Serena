//
//  localtime_r.c
//  libc
//
//  Created by Dietmar Planitzer on 8/18/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include <time.h>
#include <errno.h>
#include "gregorian_calendar.h"


struct tm *localtime_r(const time_t *timer, struct tm *buf)
{
    //XXX make this work correctly with negative time values
    int y;
    unsigned m, d;
    time_t now = *timer;

    if (now < 0) {
        errno = EOVERFLOW;
        return NULL;
    }
    
    const days_t days = (days_t)(now / 86400);
    now -= (time_t)days * 86400;
    const time_t hours = now / 3600;
    now -= hours * 3600;
    const time_t mins = now / 60;
    now -= mins * 60;
    const time_t secs = now;

    __gc_civil_from_days(days, &y, &m, &d);
    buf->tm_year = y - 1900;
    buf->tm_mon = m - 1;
    buf->tm_mday = d;
    buf->tm_hour = hours;
    buf->tm_min = mins;
    buf->tm_sec = secs;
    buf->tm_wday = __gc_weekday_from_days(days);
    buf->tm_yday = days - __gc_days_from_civil(y, 1, 1);
    buf->tm_isdst = 0;

    return buf;
}
