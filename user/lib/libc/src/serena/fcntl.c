//
//  fcntl.c
//  libc
//
//  Created by Dietmar Planitzer on 5/17/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <fcntl.h>
#include <stdarg.h>
#include <ext/errno.h>
#include <kpi/syscall.h>


int fcntl(int fd, int cmd, ...)
{
    va_list ap;
    int r = -1;

    va_start(ap, cmd);
    const int err = (errno_t)_syscall(SC_fcntl, fd, cmd, &r, ap);
    va_end(ap);
    return r;
}
