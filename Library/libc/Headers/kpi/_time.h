//
//  kpi/time.h
//  libc
//
//  Created by Dietmar Planitzer on 2/5/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef _KPI_TIME_H
#define _KPI_TIME_H 1

#ifdef _POSIX_SOURCE
// Type to identify a clock
typedef int clockid_t;
#endif

// Big enough to hold a time value of up to a bit more than 49.5 days on LP32 systems
typedef unsigned long   clock_t;

// Seconds since 00:00, Jan 1st 1970 UTC
typedef long time_t;

struct timespec {
    time_t  tv_sec;
    long    tv_nsec;    // 0..<1billion
};

// Milliseconds
typedef long mseconds_t;

// Microseconds
typedef long useconds_t;

#endif /* _KPI_TIME_H */
