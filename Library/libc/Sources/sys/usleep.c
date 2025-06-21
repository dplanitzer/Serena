//
//  usleep.c
//  libc
//
//  Created by Dietmar Planitzer on 6/20/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <time.h>
#include <kpi/syscall.h>
#include <sys/timespec.h>


int usleep(useconds_t us)
{
    struct timespec ts;

    timespec_from_us(&ts, us);
    return (int)_syscall(SC_clock_nanosleep, CLOCK_MONOTONIC, 0, &ts, NULL);
}
