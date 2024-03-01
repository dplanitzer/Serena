//
//  Directory.c
//  libsystem
//
//  Created by Dietmar Planitzer on 2/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include <System/Directory.h>
#include <System/_syscall.h>


errno_t Directory_Create(const char* _Nonnull path, FilePermissions mode)
{
    return (errno_t)_syscall(SC_mkdir, path, (uint32_t)mode);
}

errno_t Directory_Open(const char* _Nonnull path, int* _Nonnull ioc)
{
    return (errno_t)_syscall(SC_opendir, path, ioc);
}

errno_t Directory_Read(int ioc, DirectoryEntry* _Nonnull entries, size_t nEntriesToRead, ssize_t* _Nonnull nReadEntries)
{
    ssize_t nBytesRead;
    const errno_t  err = _syscall(SC_read, ioc, entries, nEntriesToRead * sizeof(DirectoryEntry), &nBytesRead);
    *nReadEntries = nBytesRead / sizeof(DirectoryEntry);
    return err;
}
