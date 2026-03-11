//
//  utime.h
//  libc
//
//  Created by Dietmar Planitzer on 5/17/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _SYS_UTIME_H
#define _SYS_UTIME_H 1

#include <_cmndef.h>
#include <serena/types.h>

__CPP_BEGIN

struct utimbuf {
    time_t  actime;
    time_t  modtime;
};

extern int utime(const char* _Nonnull path, const struct utimbuf* _Nullable times);

__CPP_END

#endif /* _SYS_UTIME_H */
