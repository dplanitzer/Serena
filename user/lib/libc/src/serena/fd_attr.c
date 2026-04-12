//
//  fd_attr.c
//  libc
//
//  Created by Dietmar Planitzer on 5/15/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <serena/fd.h>
#include <kpi/syscall.h>

int fd_attr(int fd, fs_attr_t* _Nonnull attr)
{
    return (int)_syscall(SC_fd_attr, fd, attr);
}
