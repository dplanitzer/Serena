//
//  driver.c
//  diskimage
//
//  Created by Dietmar Planitzer on 3/10/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "MonotonicClock.h"

TimeInterval MonotonicClock_GetCurrentTime(void)
{
    TimeInterval ti;

    timespec_get(&ti, TIME_UTC);
    return ti;
}
