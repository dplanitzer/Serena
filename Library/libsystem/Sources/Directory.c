//
//  Directory.c
//  libsystem
//
//  Created by Dietmar Planitzer on 2/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include <System/Directory.h>
#include <System/File.h>
#include <System/_syscall.h>


errno_t Directory_Create(const char* _Nonnull path, FilePermissions mode)
{
    return (errno_t)_syscall(SC_mkdir, path, (uint32_t)mode);
}

errno_t Directory_Open(const char* _Nonnull path, int* _Nonnull ioc)
{
    return (errno_t)_syscall(SC_opendir, path, ioc);
}

errno_t Directory_Read(int ioc, DirectoryEntry* _Nonnull entries, size_t nBytesToRead, ssize_t* _Nonnull nBytesRead)
{
    return (errno_t)_syscall(SC_read, ioc, entries, nBytesToRead, nBytesRead);
}

errno_t Directory_Rewind(int ioc)
{
    return (errno_t)_syscall(SC_seek, ioc, (off_t)0ll, NULL, kSeek_Set);
}
