//
//  fs_remove.c
//  libc
//
//  Created by Dietmar Planitzer on 5/14/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <kpi/syscall.h>
#include <serena/file.h>
#include "__readdir.h"

int fs_remove(dir_t _Nullable wd, const char* _Nonnull path)
{
    return (int)_syscall(SC_fs_unlink, (wd) ? _dir_fd(wd) : FD_CWD, path, __FS_ULNK_ANY);
}
