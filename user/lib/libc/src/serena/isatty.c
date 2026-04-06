//
//  isatty.c
//  libc
//
//  Created by Dietmar Planitzer on 5/14/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <kpi/syscall.h>
#include <serena/file.h>

int isatty(int fd)
{
    return (fcntl(fd, F_GETTYPE) == FD_TYPE_TERMINAL) ? 1 : 0;
}
