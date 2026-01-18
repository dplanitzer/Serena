//
//  utime.c
//  libc
//
//  Created by Dietmar Planitzer on 5/17/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <utime.h>
#include <sys/stat.h>


int utime(const char* _Nonnull path, const struct utimbuf* _Nullable times)
{
    struct timespec ts[2];

    if (times) {
        ts[UTIME_ACCESS].tv_sec = times->actime;
        ts[UTIME_ACCESS].tv_nsec = 0;
        ts[UTIME_MODIFICATION].tv_sec = times->actime;
        ts[UTIME_MODIFICATION].tv_nsec = 0;
    }
    else {
        ts[UTIME_ACCESS].tv_sec = 0;
        ts[UTIME_ACCESS].tv_nsec = UTIME_NOW;
        ts[UTIME_MODIFICATION].tv_sec = 0;
        ts[UTIME_MODIFICATION].tv_nsec = UTIME_NOW;
    }

    return utimens(path, ts);
}
