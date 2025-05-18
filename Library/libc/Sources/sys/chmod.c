//
//  chmod.c
//  libc
//
//  Created by Dietmar Planitzer on 5/17/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <sys/stat.h>
#include <sys/_syscall.h>


int chmod(const char* _Nonnull path, mode_t mode)
{
    return (int)_syscall(SC_chmod, path, mode);
}
