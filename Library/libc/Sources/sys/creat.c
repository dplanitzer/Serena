//
//  creat.c
//  libc
//
//  Created by Dietmar Planitzer on 5/14/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <fcntl.h>
#include <sys/_syscall.h>


errno_t creat(const char* _Nonnull path, unsigned int mode, mode_t permissions, int* _Nonnull ioc)
{
    return (errno_t)_syscall(SC_creat, path, mode, permissions, ioc);
}
