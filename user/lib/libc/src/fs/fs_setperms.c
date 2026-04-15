//
//  fs_setperms.c
//  libc
//
//  Created by Dietmar Planitzer on 5/17/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <serena/file.h>
#include <kpi/syscall.h>
#include "__readdir.h"


int fs_setperms(dir_t _Nullable wd, const char* _Nonnull path, fs_perms_t fsperms)
{
    return (int)_syscall(SC_fs_setperms, (wd) ? _dir_fd(wd) : FD_CWD, path, fsperms);
}
