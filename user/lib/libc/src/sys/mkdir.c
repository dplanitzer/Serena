//
//  mkdir.c
//  libc
//
//  Created by Dietmar Planitzer on 5/14/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <sys/stat.h>
#include <kpi/syscall.h>


int mkdir(const char* _Nonnull path, mode_t mode)
{
    return (int)_syscall(SC_mkdir, path, (uint32_t)mode);
}
