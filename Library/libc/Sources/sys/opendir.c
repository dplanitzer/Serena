//
//  opendir.c
//  libc
//
//  Created by Dietmar Planitzer on 5/15/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <dirent.h>
#include <sys/_syscall.h>
#include <kern/_varargs.h>


errno_t opendir(const char* _Nonnull path, int* _Nonnull fd)
{
    return (errno_t)_syscall(SC_opendir, path, fd);
}
