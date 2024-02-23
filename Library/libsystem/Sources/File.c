//
//  File.c
//  libsystem
//
//  Created by Dietmar Planitzer on 2/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include <System/File.h>
#include <System/_syscall.h>
#include <System/_varargs.h>


errno_t File_Create(const char* _Nonnull path, unsigned int options, FilePermissions permissions, int* _Nonnull ioc)
{
    return (errno_t)_syscall(SC_mkfile, path, options, permissions, ioc);
}

errno_t File_Open(const char* _Nonnull path, unsigned int options, int* _Nonnull ioc)
{
    return (errno_t)_syscall(SC_open, path, options, ioc);
}


errno_t File_GetPosition(int ioc, FileOffset* _Nonnull pos)
{
    return (errno_t)_syscall(SC_seek, ioc, (FileOffset)0, pos, (int)kSeek_Current);
}

errno_t File_Seek(int ioc, FileOffset offset, FileOffset* _Nullable oldpos, int whence)
{
    return (errno_t)_syscall(SC_seek, ioc, offset, oldpos, whence);
}


errno_t File_GetInfo(const char* _Nonnull path, FileInfo* _Nonnull info)
{
    return (errno_t)_syscall(SC_getfileinfo, path, info);
}

errno_t File_SetInfo(const char* _Nonnull path, MutableFileInfo* _Nonnull info)
{
    return (errno_t)_syscall(SC_setfileinfo, path, info);
}

errno_t FileChannel_GetInfo(int ioc, FileInfo* _Nonnull info)
{
    return _syscall(SC_fgetfileinfo, ioc, info);
}

errno_t FileChannel_SetInfo(int ioc, MutableFileInfo* _Nonnull info)
{
    return (errno_t)_syscall(SC_fsetfileinfo, ioc, info);
}


errno_t File_Truncate(const char *path, FileOffset length)
{
    return (errno_t)_syscall(SC_truncate, path, length);
}

errno_t FileChannel_Truncate(int ioc, FileOffset length)
{
    return _syscall(SC_ftruncate, ioc, length);
}


errno_t File_CheckAccess(const char* _Nonnull path, AccessMode mode)
{
    return (errno_t)_syscall(SC_access, mode);
}

errno_t File_Unlink(const char* _Nonnull path)
{
    return (errno_t)_syscall(SC_unlink, path);
}

errno_t File_Rename(const char* _Nonnull oldpath, const char* _Nonnull newpath)
{
    return (errno_t)_syscall(SC_rename, oldpath, newpath);
}


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
