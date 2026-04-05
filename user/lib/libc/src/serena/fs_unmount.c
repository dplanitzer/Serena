//
//  fs_unmount.c
//  libc
//
//  Created by Dietmar Planitzer on 5/15/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <serena/fs.h>
#include <kpi/syscall.h>

int fs_unmount(const char* _Nonnull atDirPath, int flags)
{
    return (int)_syscall(SC_fs_unmount, atDirPath, flags);
}
