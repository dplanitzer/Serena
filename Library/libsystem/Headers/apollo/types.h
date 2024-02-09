//
//  types.h
//  libsystem
//
//  Created by Dietmar Planitzer on 10/12/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef _SYS_TYPES_H
#define _SYS_TYPES_H 1

#include <abi/_dmdef.h>
#include <abi/_errno.h>
#include <abi/_cmndef.h>
#include <apollo/_sizedef.h>

__CPP_BEGIN

typedef long pid_t;

typedef long uid_t;
typedef long gid_t;

typedef __ssize_t ssize_t;

typedef __errno_t errno_t;

typedef long useconds_t;

typedef long time_t;

struct timespec {
    time_t  tv_sec;
    long    tv_nsec;
};

typedef long long off_t;

typedef unsigned short mode_t;

__CPP_END

#endif /* _SYS_TYPES_H */
