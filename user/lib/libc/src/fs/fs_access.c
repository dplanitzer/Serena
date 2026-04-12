//
//  fs_access.c
//  libc
//
//  Created by Dietmar Planitzer on 5/13/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <kpi/syscall.h>
#include <serena/file.h>
#include "__readdir.h"


int fs_access(dir_t* _Nullable wd, const char* _Nonnull path, int mode)
{
    return (int)_syscall(SC_fs_access, (wd) ? _dir_fd(wd) : FD_CWD, path, mode);
}
