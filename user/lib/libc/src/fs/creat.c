//
//  creat.c
//  libc
//
//  Created by Dietmar Planitzer on 5/14/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <kpi/syscall.h>
#include <serena/file.h>

int creat(const char* _Nonnull path, fs_perms_t fsperms)
{
    int fd;

    if (_syscall(SC_mkfile, path, O_WRONLY|O_TRUNC, fsperms, &fd) == 0) {
        return fd;
    }
    else {
        return -1;
    }
}
