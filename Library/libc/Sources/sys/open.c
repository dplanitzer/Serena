//
//  open.c
//  libc
//
//  Created by Dietmar Planitzer on 5/14/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <fcntl.h>
#include <stdarg.h>
#include <kpi/syscall.h>


int open(const char* _Nonnull path, int oflags, ...)
{
    va_list ap;
    int fd, r;

    va_start(ap, oflags);

    if ((oflags & O_CREAT) == O_CREAT) {
        const mode_t mode = va_arg(ap, mode_t);

        r = (int)_syscall(SC_creat, path, oflags, mode, &fd);
    }
    else {
        r = (int)_syscall(SC_open, path, oflags, &fd);
    }

    va_end(ap);
    return (r == 0) ? fd : -1;
}
