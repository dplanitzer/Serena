//
//  clock.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <time.h>
#include <System/Clock.h>


clock_t clock(void)
{
    // XXX should actually return the time since process start
    const TimeInterval ti = clock_gettime();

    return ti.tv_sec * CLOCKS_PER_SEC + ti.tv_nsec / ((1000l * 1000l * 1000l) / CLOCKS_PER_SEC);
}
