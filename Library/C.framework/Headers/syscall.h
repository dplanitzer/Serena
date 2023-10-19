//
//  syscall.h
//  Apollo
//
//  Created by Dietmar Planitzer on 9/2/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef _SYSCALL_H
#define _SYSCALL_H 1

#include <_cmndef.h>
#include <_syscall.h>
#include <errno.h>

__CPP_BEGIN

#define __failable_syscall(__r, ...) \
    __r = __syscall(__VA_ARGS__); \
    if (__r < 0) { \
        errno = -__r; \
    }

extern int __syscall(int scno, ...);

__CPP_END

#endif /* _SYSCALL_H */
