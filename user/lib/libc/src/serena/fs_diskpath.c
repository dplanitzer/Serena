//
//  fs_diskpath.c
//  libc
//
//  Created by Dietmar Planitzer on 5/15/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <serena/filesystem.h>
#include <kpi/syscall.h>

int fs_diskpath(fsid_t fsid, char* _Nonnull buf, size_t bufSize)
{
    return (int)_syscall(SC_fs_diskpath, fsid, buf, bufSize);
}
