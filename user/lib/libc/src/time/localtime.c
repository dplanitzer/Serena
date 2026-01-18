//
//  localtime.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <time.h>


struct tm *localtime(const time_t *timer)
{
    static struct tm gLocaltimeBuffer;

    return localtime_r(timer, &gLocaltimeBuffer);
}
