//
//  fd_write.c
//  libc
//
//  Created by Dietmar Planitzer on 5/14/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <kpi/syscall.h>
#include <serena/fd.h>

ssize_t fd_write(int fd, const void* _Nonnull buf, size_t nbytes)
{
    ssize_t nBytesWritten;

    if (_syscall(SC_fd_write, fd, buf, nbytes, &nBytesWritten) == 0) {
        return nBytesWritten;
    }
    else {
        return -1;
    }
}
