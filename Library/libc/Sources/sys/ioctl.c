//
//  ioctl.c
//  libc
//
//  Created by Dietmar Planitzer on 5/14/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <stdarg.h>
#include <sys/ioctl.h>
#include <sys/_syscall.h>


int ioctl(int fd, int cmd, ...)
{
    va_list ap;
    int r;

    va_start(ap, cmd);
    r = (int)_syscall(SC_ioctl, fd, cmd, ap);
    va_end(ap);

    return r;
}
