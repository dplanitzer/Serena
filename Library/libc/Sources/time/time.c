//
//  time.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <time.h>
#include <System/Clock.h>


time_t time(time_t *timer)
{
    const TimeInterval ti = clock_gettime();

    if (timer) {
        *timer = ti.tv_sec;
    }

    return ti.tv_sec;
}
