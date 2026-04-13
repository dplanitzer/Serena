//
//  fd_dup_to.c
//  libc
//
//  Created by Dietmar Planitzer on 4/12/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include <kpi/syscall.h>
#include <serena/fd.h>


int fd_dup_to(int fd, int target_fd)
{
    int new_fd;

    if (_syscall(SC_fd_dup_to, fd, target_fd, &new_fd) == 0) {
        return new_fd;
    }
    else {
        return -1;
    }
}
