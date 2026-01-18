//
//  utime.h
//  libc
//
//  Created by Dietmar Planitzer on 5/17/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _UTIME_H
#define _UTIME_H 1

#if ___STDC_HOSTED__ != 1
#error "not supported in freestanding mode"
#endif

#include <_cmndef.h>
#include <sys/types.h>

__CPP_BEGIN

struct utimbuf {
    time_t  actime;
    time_t  modtime;
};

extern int utime(const char* _Nonnull path, const struct utimbuf* _Nullable times);

__CPP_END

#endif /* _UTIME_H */
