//
//  fstat.c
//  libc
//
//  Created by Dietmar Planitzer on 5/15/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <serena/file.h>
#include <kpi/syscall.h>

int fstat(int fd, struct stat* _Nonnull info)
{
    return (int)_syscall(SC_fstat, fd, info);
}
