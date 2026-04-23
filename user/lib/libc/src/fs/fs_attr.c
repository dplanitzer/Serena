//
//  fs_attr.c
//  libc
//
//  Created by Dietmar Planitzer on 5/15/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <serena/file.h>
#include <kpi/syscall.h>
#include <__readdir.h>

int fs_attr(dir_t _Nullable wd, const char* _Nonnull path, fs_attr_t* _Nonnull attr)
{
    return (int)_syscall(SC_fs_attr, (wd) ? _dir_fd(wd) : FD_CWD, path, attr);
}
