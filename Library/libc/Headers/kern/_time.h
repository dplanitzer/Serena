//
//  time.h
//  libsystem
//
//  Created by Dietmar Planitzer on 2/5/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef _SYS_TIME_H
#define _SYS_TIME_H 1

#include <kern/_cmndef.h>
#include <machine/abi/_size.h>

__CPP_BEGIN

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

__CPP_END

#endif /* _SYS_TIME_H */
