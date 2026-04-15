//
//  fs_open.c
//  libc
//
//  Created by Dietmar Planitzer on 5/14/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <kpi/syscall.h>
#include <serena/file.h>
#include "__readdir.h"

int fs_open(dir_t _Nullable wd, const char* _Nonnull path, int oflags)
{
    int fd;
    const int r = (int)_syscall(SC_fs_open, (wd) ? _dir_fd(wd) : FD_CWD, path, oflags, &fd);

    return (r == 0) ? fd : -1;
}
