//
//  fs_property.c
//  libc
//
//  Created by Dietmar Planitzer on 5/15/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <serena/filesystem.h>
#include <kpi/syscall.h>

int fs_property(fsid_t fsid, int flavor, char* _Nonnull buf, size_t bufSize)
{
    return (int)_syscall(SC_fs_property, fsid, flavor, buf, bufSize);
}
