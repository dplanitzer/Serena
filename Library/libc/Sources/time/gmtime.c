//
//  gmtime.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <time.h>


static struct tm gGmTimeBuffer;

struct tm *gmtime(const time_t *timer)
{
    return gmtime_r(timer, &gGmTimeBuffer);
}
