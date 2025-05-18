//
//  stat.c
//  libc
//
//  Created by Dietmar Planitzer on 5/15/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <sys/stat.h>
#include <sys/_syscall.h>


errno_t getfinfo(const char* _Nonnull path, struct stat* _Nonnull info)
{
    return (errno_t)_syscall(SC_getinfo, path, info);
}

errno_t fgetfinfo(int ioc, struct stat* _Nonnull info)
{
    return _syscall(SC_fgetinfo, ioc, info);
}
