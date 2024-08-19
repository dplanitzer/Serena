//
//  asctime.c
//  libc
//
//  Created by Dietmar Planitzer on 2/27/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include <time.h>
#include <stdio.h>


char *ctime(const time_t *timer)
{
    struct tm* lt = localtime(timer);

    return (lt) ? asctime(lt) : "";
}
