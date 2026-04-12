//
//  fd_info.c
//  libc
//
//  Created by Dietmar Planitzer on 4/11/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include <serena/fd.h>
#include <kpi/syscall.h>

int fd_info(int fd, int flavor, fd_info_ref _Nonnull info)
{
    return (int)_syscall(SC_fd_info, fd, flavor, info);
}
