//
//  fs_create_file.c
//  libc
//
//  Created by Dietmar Planitzer on 4/14/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include <kpi/syscall.h>
#include <serena/file.h>
#include <__readdir.h>

int fs_create_file(dir_t _Nullable wd, const char* _Nonnull path, int oflags, fs_perms_t fsperms)
{
    int fd;
    const int r = (int)_syscall(SC_fs_create_file, (wd) ? _dir_fd(wd) : FD_CWD, path, oflags, fsperms, &fd);

    return (r == 0) ? fd : -1;
}
