//
//  types.h
//  Apollo
//
//  Created by Dietmar Planitzer on 10/12/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef _APOLLO_TYPES_H
#define _APOLLO_TYPES_H 1

#include <_cmndef.h>
#include <_dmdef.h>
#include <_errdef.h>
#include <_sizedef.h>

__CPP_BEGIN

typedef long pid_t;

typedef long uid_t;
typedef long gid_t;

typedef __ssize_t ssize_t;

typedef __errno_t errno_t;

typedef long useconds_t;

typedef long long off_t;

__CPP_END

#endif /* _APOLLO_TYPES_H */
