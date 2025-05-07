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
    TimeInterval ts;
    
    clock_gettime(CLOCK_UPTIME, &ts);
    if (timer) {
        *timer = ts.tv_sec;
    }

    return ts.tv_sec;
}
