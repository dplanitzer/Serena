//
//  read.c
//  libc
//
//  Created by Dietmar Planitzer on 5/14/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <kpi/syscall.h>
#include <serena/fd.h>


ssize_t read(int fd, void* _Nonnull buf, size_t nbytes)
{
    ssize_t nBytesRead;

    if (_syscall(SC_read, fd, buf, nbytes, &nBytesRead) == 0) {
        return nBytesRead;
    }
    else {
        return -1;
    }
}
