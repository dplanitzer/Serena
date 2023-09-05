//
//  errno.h
//  Apollo
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef _ERRNO_H
#define _ERRNO_H 1

#include <_cmndef.h>

__CPP_BEGIN

#define ENOMEM      1
#define ENOMEDIUM   2
#define EDISKCHANGE 3
#define ETIMEDOUT   4
#define ENODEV      5
#define EPARAM      6
#define ERANGE      7
#define EINTR       8
#define EAGAIN      9
#define EPIPE       10
#define EBUSY       11
#define ENOSYS      12
#define EINVAL      13
#define EIO         14
#define EPERM       15
#define EDEADLK     16
#define EDOM        17
#define ENOEXEC     18
#define E2BIG       19
#define ENOENT      20
#define ENOTBLK     21

#define __EFIRST    1
#define __ELAST     21


// XXX make this per VP
extern int errno;

__CPP_END

#endif /* _ERRNO_H */
