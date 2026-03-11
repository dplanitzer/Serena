//
//  mount.c
//  libc
//
//  Created by Dietmar Planitzer on 5/15/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <serena/mount.h>
#include <kpi/syscall.h>


int mount(const char* _Nonnull objectType, const char* _Nonnull objectName, const char* _Nonnull atDirPath, const char* _Nonnull params)
{
    return (int)_syscall(SC_mount, objectType, objectName, atDirPath, params);
}
