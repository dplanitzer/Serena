//
//  fd_setflags.c
//  libc
//
//  Created by Dietmar Planitzer on 4/12/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include <serena/fd.h>
#include <kpi/syscall.h>

fd_flags_t fd_setflags(int fd, int op, fd_flags_t flags)
{
    fd_flags_t old_flags;

    if(_syscall(SC_fd_setflags, fd, op, flags, &old_flags) == 0) {
        return old_flags;
    }
    else {
        return -1;
    }
}
