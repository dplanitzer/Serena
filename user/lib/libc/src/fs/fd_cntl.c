//
//  fd_cntl.c
//  libc
//
//  Created by Dietmar Planitzer on 5/14/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <stdarg.h>
#include <serena/fd.h>
#include <kpi/syscall.h>


int fd_cntl(int fd, int cmd, ...)
{
    va_list ap;
    int r;

    va_start(ap, cmd);
    r = (int)_syscall(SC_fd_cntl, fd, cmd, ap);
    va_end(ap);

    return r;
}
