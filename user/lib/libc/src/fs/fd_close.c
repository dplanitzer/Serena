//
//  fd_close.c
//  libc
//
//  Created by Dietmar Planitzer on 5/14/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <kpi/syscall.h>
#include <serena/fd.h>

int fd_close(int fd)
{
    return (int)_syscall(SC_fd_close, fd);
}
