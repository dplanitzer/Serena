//
//  fd_flags.c
//  libc
//
//  Created by Dietmar Planitzer on 4/11/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include <serena/fd.h>
#include <kpi/syscall.h>

fd_flags_t fd_flags(int fd)
{
    fd_flags_t flags;

    if (_syscall(SC_fd_flags, fd, &flags) == 0) {
        return flags;
    }
    else {
        return -1;
    }
}
