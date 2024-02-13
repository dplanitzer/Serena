//
//  File.c
//  libsystem
//
//  Created by Dietmar Planitzer on 2/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include <apollo/File.h>
#include <apollo/_syscall.h>
#include <apollo/_varargs.h>


errno_t File_Create(const char* path, unsigned int options, FilePermissions permissions, int* fd)
{
    return (errno_t)_syscall(SC_mkfile, path, options, permissions, fd);
}

errno_t File_Open(const char *path, unsigned int options, int* fd)
{
    return (errno_t)_syscall(SC_open, path, options, fd);
}


errno_t File_GetPosition(int fd, FileOffset* pos)
{
    return (errno_t)_syscall(SC_seek, fd, (FileOffset)0, pos, (int)kSeek_Current);
}

errno_t File_Seek(int fd, FileOffset offset, FileOffset *oldpos, int whence)
{
    return (errno_t)_syscall(SC_seek, fd, offset, oldpos, whence);
}


errno_t File_GetInfo(const char* path, FileInfo* info)
{
    return (errno_t)_syscall(SC_getfileinfo, path, info);
}

errno_t File_SetInfo(const char* path, MutableFileInfo* info)
{
    return (errno_t)_syscall(SC_setfileinfo, path, info);
}

errno_t FileChannel_GetInfo(int fd, FileInfo* info)
{
    return _syscall(SC_fgetfileinfo, fd, info);
}

errno_t FileChannel_SetInfo(int fd, MutableFileInfo* info)
{
    return (errno_t)_syscall(SC_fsetfileinfo, fd, info);
}


errno_t File_Truncate(const char *path, FileOffset length)
{
    return (errno_t)_syscall(SC_truncate, path, length);
}

errno_t FileChannel_Truncate(int fd, FileOffset length)
{
    return _syscall(SC_ftruncate, fd, length);
}


errno_t File_CheckAccess(const char* path, AccessMode mode)
{
    return (errno_t)_syscall(SC_access, mode);
}

errno_t File_Unlink(const char* path)
{
    return (errno_t)_syscall(SC_unlink, path);
}

errno_t File_Rename(const char* oldpath, const char* newpath)
{
    return (errno_t)_syscall(SC_rename, oldpath, newpath);
}


errno_t Directory_Create(const char* path, FilePermissions mode)
{
    return (errno_t)_syscall(SC_mkdir, path, (uint32_t)mode);
}

errno_t Directory_Open(const char* path, int* fd)
{
    return (errno_t)_syscall(SC_opendir, path, fd);
}

errno_t Directory_Read(int fd, DirectoryEntry* entries, size_t nEntriesToRead, ssize_t* nReadEntries)
{
    ssize_t nBytesRead;
    const errno_t  err = _syscall(SC_read, fd, entries, nEntriesToRead * sizeof(DirectoryEntry), &nBytesRead);
    *nReadEntries = nBytesRead / sizeof(DirectoryEntry);
    return err;
}
