//
//  unlink.c
//  libc
//
//  Created by Dietmar Planitzer on 5/14/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <kpi/syscall.h>
#include <serena/file.h>
#include <serena/process.h>

int unlink(const char* _Nonnull path)
{
    return (int)_syscall(SC_unlink, path, __ULNK_FIL_ONLY);
}
