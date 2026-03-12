//
//  fs_getdisk.c
//  libc
//
//  Created by Dietmar Planitzer on 5/15/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <serena/fs.h>
#include <kpi/syscall.h>

int fs_getdisk(fsid_t fsid, char* _Nonnull buf, size_t bufSize)
{
    return (int)_syscall(SC_fsgetdisk, fsid, buf, bufSize);
}
