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


errno_t File_Create(const char* _Nonnull path, unsigned int mode, FilePermissions permissions, int* _Nonnull ioc)
{
    return (errno_t)_syscall(SC_mkfile, path, mode, permissions, ioc);
}

errno_t File_Open(const char* _Nonnull path, unsigned int mode, int* _Nonnull ioc)
{
    return (errno_t)_syscall(SC_open, path, mode, ioc);
}


errno_t File_GetPosition(int ioc, off_t* _Nonnull pos)
{
    return (errno_t)_syscall(SC_seek, ioc, (off_t)0ll, pos, (int)kSeek_Current);
}

errno_t File_Seek(int ioc, off_t offset, off_t* _Nullable oldpos, int whence)
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


errno_t File_Truncate(const char *path, off_t length)
{
    return (errno_t)_syscall(SC_truncate, path, length);
}

errno_t FileChannel_Truncate(int ioc, off_t length)
{
    return _syscall(SC_ftruncate, ioc, length);
}


errno_t File_CheckAccess(const char* _Nonnull path, AccessMode mode)
{
    return (errno_t)_syscall(SC_access, path, mode);
}

errno_t File_Unlink(const char* _Nonnull path)
{
    return (errno_t)_syscall(SC_unlink, path);
}

errno_t File_Rename(const char* _Nonnull oldpath, const char* _Nonnull newpath)
{
    return (errno_t)_syscall(SC_rename, oldpath, newpath);
}
