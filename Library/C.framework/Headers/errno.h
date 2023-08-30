//
//  errno.h
//  Apollo
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef _ERRNO_H
#define _ERRNO_H 1

#define EOK         0
#define ENOMEM      1
#define ENODATA     2
#define ENOTCDF     3
#define ENOBOOT     4
#define ENODRIVE    5
#define EDISKCHANGE 6
#define ETIMEDOUT   7
#define ENODEV      8
#define EPARAM      9
#define ERANGE      10
#define EINTR       11
#define EAGAIN      12
#define EPIPE       13
#define EBUSY       14
#define ENOSYS      15
#define EINVAL      16
#define EIO         17
#define EPERM       18
#define EDEADLK     19
#define EDOM        20
#define EILSEQ      21
#define ENOEXEC     22
#define E2BIG       23


// XXX make this per VP
extern int errno;

#endif /* _ERRNO_H */
