//
//  fs_rename.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <serena/file.h>
#include <kpi/syscall.h>
#include "__readdir.h"


int fs_rename(dir_t* _Nullable owd, const char* _Nonnull oldpath, dir_t* _Nullable nwd, const char* _Nonnull newpath)
{
    return (int)_syscall(SC_fs_rename, (owd) ? _dir_fd(owd) : FD_CWD, oldpath, (nwd) ? _dir_fd(nwd) : FD_CWD, newpath);
}
