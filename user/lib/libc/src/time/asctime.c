//
//  asctime.c
//  libc
//
//  Created by Dietmar Planitzer on 2/27/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include <time.h>
#include <stdio.h>


char *asctime(const struct tm *timeptr)
{
    static char __gAscTimeBuffer[26];

    return asctime_r(timeptr, __gAscTimeBuffer);
}
