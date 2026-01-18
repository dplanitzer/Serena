//
//  unmount.c
//  libc
//
//  Created by Dietmar Planitzer on 5/15/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <sys/mount.h>
#include <kpi/syscall.h>


int unmount(const char* _Nonnull atDirPath, UnmountOptions options)
{
    return (int)_syscall(SC_unmount, atDirPath, options);
}
