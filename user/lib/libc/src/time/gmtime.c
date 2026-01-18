//
//  gmtime.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <time.h>


struct tm *gmtime(const time_t *timer)
{
    static struct tm gGmTimeBuffer;

    return gmtime_r(timer, &gGmTimeBuffer);
}
