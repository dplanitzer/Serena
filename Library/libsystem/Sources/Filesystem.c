//
//  Filesystem.c
//  libsystem
//
//  Created by Dietmar Planitzer on 12/15/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include <System/Filesystem.h>
#include <System/_syscall.h>
#include <System/_varargs.h>


errno_t Mount(const char* _Nonnull containerPath, const char* _Nonnull atDirPath, const void* _Nullable params, size_t paramsSize)
{
    return (errno_t)_syscall(SC_mount, containerPath, atDirPath, params, paramsSize);
}

errno_t Unmount(const char* _Nonnull atDirPath, uint32_t options)
{
    return (errno_t)_syscall(SC_unmount, atDirPath, options);
}
