//
//  rename.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <stdio.h>
#include <kpi/syscall.h>


int rename(const char* _Nonnull oldpath, const char* _Nonnull newpath)
{
    return (int)_syscall(SC_fs_rename, -1, oldpath, -1, newpath);
}
