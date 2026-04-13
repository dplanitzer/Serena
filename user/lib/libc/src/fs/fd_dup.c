//
//  fd_dup.c
//  libc
//
//  Created by Dietmar Planitzer on 4/12/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include <kpi/syscall.h>
#include <serena/fd.h>


int fd_dup(int fd, int min_fd)
{
    int new_fd;

    if (_syscall(SC_fd_dup, fd, min_fd, &new_fd) == 0) {
        return new_fd;
    }
    else {
        return -1;
    }
}
