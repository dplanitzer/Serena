//
//  readdir.c
//  libc
//
//  Created by Dietmar Planitzer on 5/15/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <dirent.h>
#include <sys/_syscall.h>


errno_t readdir(DIR* _Nonnull dir, dirent_t* _Nonnull entries, size_t nBytesToRead, ssize_t* _Nonnull nBytesRead)
{
    return (errno_t)_syscall(SC_read, (int)dir - __DIR_BASE, entries, nBytesToRead, nBytesRead);
}
