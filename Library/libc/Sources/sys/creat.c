//
//  creat.c
//  libc
//
//  Created by Dietmar Planitzer on 5/14/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <fcntl.h>
#include <sys/_syscall.h>
#include <System/_varargs.h>


errno_t creat(const char* _Nonnull path, unsigned int mode, FilePermissions permissions, int* _Nonnull ioc)
{
    return (errno_t)_syscall(SC_creat, path, mode, permissions, ioc);
}
