//
//  rmdir.c
//  libc
//
//  Created by Dietmar Planitzer on 6/10/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <kpi/syscall.h>
#include <serena/file.h>
#include <serena/process.h>

int rmdir(const char* _Nonnull path)
{
    return (int)_syscall(SC_unlink, path, __ULNK_DIR_ONLY);
}
