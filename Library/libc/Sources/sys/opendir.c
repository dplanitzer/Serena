//
//  opendir.c
//  libc
//
//  Created by Dietmar Planitzer on 5/15/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <dirent.h>
#include <stddef.h>
#include <sys/_syscall.h>


DIR* _Nullable opendir(const char* _Nonnull path)
{
    int fd = -1;
    const errno_t err = (errno_t)_syscall(SC_opendir, path, &fd);

    return (err == EOK) ? (DIR*)(fd + __DIR_BASE) : NULL;
}
