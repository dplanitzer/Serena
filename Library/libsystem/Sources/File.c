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


errno_t mkfile(const char* _Nonnull path, unsigned int mode, FilePermissions permissions, int* _Nonnull ioc)
{
    return (errno_t)_syscall(SC_mkfile, path, mode, permissions, ioc);
}

errno_t open(const char* _Nonnull path, unsigned int mode, int* _Nonnull ioc)
{
    return (errno_t)_syscall(SC_open, path, mode, ioc);
}


errno_t getfinfo(const char* _Nonnull path, finfo_t* _Nonnull info)
{
    return (errno_t)_syscall(SC_getinfo, path, info);
}

errno_t setfinfo(const char* _Nonnull path, fmutinfo_t* _Nonnull info)
{
    return (errno_t)_syscall(SC_setinfo, path, info);
}

errno_t fgetfinfo(int ioc, finfo_t* _Nonnull info)
{
    return _syscall(SC_fgetinfo, ioc, info);
}

errno_t fsetfinfo(int ioc, fmutinfo_t* _Nonnull info)
{
    return (errno_t)_syscall(SC_fsetinfo, ioc, info);
}


errno_t os_truncate(const char *path, off_t length)
{
    return (errno_t)_syscall(SC_truncate, path, length);
}

errno_t ftruncate(int ioc, off_t length)
{
    return _syscall(SC_ftruncate, ioc, length);
}


errno_t unlink(const char* _Nonnull path)
{
    return (errno_t)_syscall(SC_unlink, path);
}

errno_t os_rename(const char* _Nonnull oldpath, const char* _Nonnull newpath)
{
    return (errno_t)_syscall(SC_rename, oldpath, newpath);
}
