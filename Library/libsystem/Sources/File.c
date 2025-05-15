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
