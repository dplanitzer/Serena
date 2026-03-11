//
//  truncate.c
//  libc
//
//  Created by Dietmar Planitzer on 5/14/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <kpi/syscall.h>
#include <serena/process.h>

int truncate(const char *path, off_t length)
{
    return (int)_syscall(SC_truncate, path, length);
}
