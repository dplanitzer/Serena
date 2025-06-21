//
//  sleep.c
//  libc
//
//  Created by Dietmar Planitzer on 6/20/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <time.h>
#include <kpi/syscall.h>
#include <sys/timespec.h>


unsigned int sleep(unsigned int seconds)
{
    struct timespec ts, rt;

    timespec_from_sec(&ts, seconds);
    if (_syscall(SC_clock_nanosleep, CLOCK_MONOTONIC, 0, &ts, &rt) == 0) {
        return 0;
    }
    else {
        return rt.tv_sec;
    }
}
