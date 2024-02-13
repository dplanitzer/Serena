//
//  types.h
//  libsystem
//
//  Created by Dietmar Planitzer on 10/12/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef _SYS_TYPES_H
#define _SYS_TYPES_H 1

#include <System/_dmdef.h>
#include <System/_errno.h>
#include <System/_cmndef.h>
#include <System/_inttypes.h>
#include <System/_sizedef.h>

__CPP_BEGIN

// Various Kernel API types
typedef int         ProcessId;

typedef int32_t     FilesystemId;
typedef int32_t     InodeId;    // XXX should probably be 64bit

typedef uint16_t    FilePermissions;
typedef int8_t      FileType;
typedef int64_t     FileOffset;

typedef uint32_t    UserId;
typedef uint32_t    GroupId;

typedef __ssize_t ssize_t;

#define SSIZE_MIN  __SSIZE_MIN
#define SSIZE_MAX  __SSIZE_MAX
#define SSIZE_WIDTH __SSIZE_WIDTH

#define SIZE_MAX __SIZE_MAX
#define SIZE_WIDTH __SIZE_WIDTH

typedef long useconds_t;

typedef long time_t;

struct timespec {
    time_t  tv_sec;
    long    tv_nsec;
};

struct _time_interval_t {
    long    seconds;
    long    nanoseconds;        // 0..<1billion
};

__CPP_END

#endif /* _SYS_TYPES_H */
