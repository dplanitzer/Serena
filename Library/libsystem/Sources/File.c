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


errno_t os_mkfile(const char* _Nonnull path, unsigned int mode, FilePermissions permissions, int* _Nonnull ioc)
{
    return (errno_t)_syscall(SC_mkfile, path, mode, permissions, ioc);
}

errno_t os_open(const char* _Nonnull path, unsigned int mode, int* _Nonnull ioc)
{
    return (errno_t)_syscall(SC_open, path, mode, ioc);
}


errno_t os_tell(int ioc, off_t* _Nonnull pos)
{
    return (errno_t)_syscall(SC_seek, ioc, (off_t)0ll, pos, (int)kSeek_Current);
}

errno_t os_seek(int ioc, off_t offset, off_t* _Nullable oldpos, int whence)
{
    return (errno_t)_syscall(SC_seek, ioc, offset, oldpos, whence);
}


errno_t os_getinfo(const char* _Nonnull path, FileInfo* _Nonnull info)
{
    return (errno_t)_syscall(SC_getinfo, path, info);
}

errno_t os_setinfo(const char* _Nonnull path, MutableFileInfo* _Nonnull info)
{
    return (errno_t)_syscall(SC_setinfo, path, info);
}

errno_t os_fgetinfo(int ioc, FileInfo* _Nonnull info)
{
    return _syscall(SC_fgetinfo, ioc, info);
}

errno_t os_fsetinfo(int ioc, MutableFileInfo* _Nonnull info)
{
    return (errno_t)_syscall(SC_fsetinfo, ioc, info);
}


errno_t os_truncate(const char *path, off_t length)
{
    return (errno_t)_syscall(SC_truncate, path, length);
}

errno_t os_ftruncate(int ioc, off_t length)
{
    return _syscall(SC_ftruncate, ioc, length);
}


errno_t os_access(const char* _Nonnull path, AccessMode mode)
{
    return (errno_t)_syscall(SC_access, path, mode);
}

errno_t os_unlink(const char* _Nonnull path)
{
    return (errno_t)_syscall(SC_unlink, path);
}

errno_t os_rename(const char* _Nonnull oldpath, const char* _Nonnull newpath)
{
    return (errno_t)_syscall(SC_rename, oldpath, newpath);
}
