//
//  fs_open.c
//  libc
//
//  Created by Dietmar Planitzer on 5/14/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <kpi/syscall.h>
#include <serena/file.h>

int fs_open(const char* _Nonnull path, int oflags)
{
    int fd;
    const int r = (int)_syscall(SC_fs_open, path, oflags, &fd);

    return (r == 0) ? fd : -1;
}
