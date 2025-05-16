//
//  mkdir.c
//  libc
//
//  Created by Dietmar Planitzer on 5/14/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <sys/stat.h>
#include <sys/_syscall.h>
#include <kern/_varargs.h>


int mkdir(const char* _Nonnull path, FilePermissions mode)
{
    return (int)_syscall(SC_mkdir, path, (uint32_t)mode);
}
