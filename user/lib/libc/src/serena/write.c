//
//  write.c
//  libc
//
//  Created by Dietmar Planitzer on 5/14/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <unistd.h>
#include <kpi/syscall.h>


ssize_t write(int fd, const void* _Nonnull buf, size_t nbytes)
{
    ssize_t nBytesWritten;

    if (_syscall(SC_write, fd, buf, nbytes, &nBytesWritten) == 0) {
        return nBytesWritten;
    }
    else {
        return -1;
    }
}
