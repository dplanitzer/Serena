//
//  ctime_r.c
//  libc
//
//  Created by Dietmar Planitzer on 1/9/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <time.h>
#include <stdio.h>


char *ctime_r(const time_t *timer, char *buf)
{
    struct tm lt;

    if (localtime_r(timer, &lt) != NULL) {
        return asctime_r(&lt, buf);
    }
    else {
        return "";
    }
}
