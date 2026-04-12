//
//  fd_setflags.c
//  libc
//
//  Created by Dietmar Planitzer on 4/12/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include <serena/fd.h>
#include <kpi/syscall.h>

int fd_setflags(int fd, int op, int flags)
{
    return (int)_syscall(SC_fd_setflags, fd, op, flags);
}
