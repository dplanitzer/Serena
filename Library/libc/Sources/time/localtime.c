//
//  localtime.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <time.h>


static struct tm gLocaltimeBuffer;

struct tm *localtime(const time_t *timer)
{
    return localtime_r(timer, &gLocaltimeBuffer);
}
