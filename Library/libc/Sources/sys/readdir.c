//
//  readdir.c
//  libc
//
//  Created by Dietmar Planitzer on 5/15/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <dirent.h>
#include <sys/_syscall.h>
#include <System/_varargs.h>


errno_t readdir(int ioc, dirent_t* _Nonnull entries, size_t nBytesToRead, ssize_t* _Nonnull nBytesRead)
{
    return (errno_t)_syscall(SC_read, ioc, entries, nBytesToRead, nBytesRead);
}
