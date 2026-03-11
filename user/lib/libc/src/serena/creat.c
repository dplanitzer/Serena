//
//  creat.c
//  libc
//
//  Created by Dietmar Planitzer on 5/14/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <fcntl.h>
#include <kpi/syscall.h>


int creat(const char* _Nonnull path, mode_t mode)
{
    int fd;

    if (_syscall(SC_mkfile, path, O_WRONLY|O_TRUNC, mode, &fd) == 0) {
        return fd;
    }
    else {
        return -1;
    }
}
