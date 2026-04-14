//
//  clock.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <time.h>
#include <serena/clock.h>

clock_t clock(void)
{
    // XXX should actually return the time since process start
    nanotime_t ts;
    
    clock_time(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * CLOCKS_PER_SEC + ts.tv_nsec / ((1000l * 1000l * 1000l) / CLOCKS_PER_SEC);
}
