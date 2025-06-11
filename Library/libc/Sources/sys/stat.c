//
//  stat.c
//  libc
//
//  Created by Dietmar Planitzer on 5/15/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <sys/stat.h>
#include <kpi/syscall.h>


int stat(const char* _Nonnull path, struct stat* _Nonnull info)
{
    return (int)_syscall(SC_stat, path, info);
}

int fstat(int ioc, struct stat* _Nonnull info)
{
    return (int)_syscall(SC_fstat, ioc, info);
}
