//
//  fd_type.c
//  libc
//
//  Created by Dietmar Planitzer on 4/11/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include <serena/fd.h>
#include <kpi/syscall.h>

int fd_type(int fd)
{
    fd_basic_info_t info;

    if (_syscall(SC_fd_info, fd, FD_INFO_BASIC, &info) == 0) {
        return info.type;
    }
    else {
        return FD_TYPE_INVALID;
    }
}
