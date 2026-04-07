//
//  fd_truncate.c
//  libc
//
//  Created by Dietmar Planitzer on 5/14/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <kpi/syscall.h>
#include <serena/fd.h>

int fd_truncate(int fd, off_t length)
{
    return (int)_syscall(SC_fd_truncate, fd, length);
}
