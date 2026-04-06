//
//  remove.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <stdio.h>
#include <kpi/syscall.h>
#include <serena/file.h>

int remove(const char* _Nonnull path)
{
    return (int)_syscall(SC_fs_unlink, -1, path, __ULNK_ANY);
}
