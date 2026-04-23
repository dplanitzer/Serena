//
//  fs_setowner.c
//  libc
//
//  Created by Dietmar Planitzer on 5/14/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <kpi/syscall.h>
#include <serena/file.h>
#include <__readdir.h>


int fs_setowner(dir_t _Nullable wd, const char* _Nonnull path, uid_t uid, gid_t gid)
{
    return (int)_syscall(SC_fs_setowner, (wd) ? _dir_fd(wd) : FD_CWD, path, uid, gid);
}
