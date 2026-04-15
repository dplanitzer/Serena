//
//  fs_settimes.c
//  libc
//
//  Created by Dietmar Planitzer on 5/14/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <serena/file.h>
#include <kpi/syscall.h>
#include "__readdir.h"


int fs_settimes(dir_t _Nullable wd, const char* _Nonnull path, const nanotime_t times[_Nullable 2])
{
    return (int)_syscall(SC_fs_settimes, (wd) ? _dir_fd(wd) : FD_CWD, path, times);
}
