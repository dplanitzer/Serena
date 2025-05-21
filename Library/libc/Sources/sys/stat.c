//
//  stat.c
//  libc
//
//  Created by Dietmar Planitzer on 5/15/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <sys/stat.h>
#include <sys/_syscall.h>
#include <sys/errno.h>


errno_t stat(const char* _Nonnull path, struct stat* _Nonnull info)
{
    return (errno_t)_syscall(SC_stat, path, info);
}

errno_t fstat(int ioc, struct stat* _Nonnull info)
{
    return _syscall(SC_fstat, ioc, info);
}
