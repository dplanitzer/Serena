//
//  time.h
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef _TIME_H
#define _TIME_H 1

#include <System/_cmndef.h>
#include <System/_null.h>
#include <System/abi/_size.h>
#include <System/_time.h>

__CPP_BEGIN

// ms resolution
#define CLOCKS_PER_SEC  1000l

// Big enough to hold a time value of up to a bit more than 49.5 days on LP32 systems
typedef unsigned long   clock_t;

// Calendar time
struct tm {
    int tm_sec;     // Seconds [0, 60]
    int tm_min;     // Minutes [0, 59]
    int tm_hour;    // Hours (since midnight) [0, 23]
    int tm_mday;    // Day of the month [1, 31]
    int tm_mon;     // Months (since January) [0, 11]
    int tm_year;    // Years since 1900
    int tm_wday;    // Days since Sunday [0, 6]
    int tm_yday;    // Days since January 1 [0, 365]
    int tm_isdst;   // Daylight Saving Time indicator
};


extern clock_t clock(void);
extern time_t time(time_t *timer);


extern time_t mktime(struct tm *timeptr);

extern struct tm *localtime(const time_t *timer);
extern struct tm *localtime_r(const time_t *timer, struct tm *buf);

extern struct tm *gmtime(const time_t *timer);
extern struct tm *gmtime_r(const time_t *timer, struct tm *buf);


extern char *asctime(const struct tm *timeptr);
extern char *ctime(const time_t *timer);

extern size_t strftime(char * restrict s, size_t maxsize, const char * restrict format, const struct tm * restrict timeptr);


extern double difftime(time_t time1, time_t time0);

__CPP_END

#endif /* _TIME_H */
