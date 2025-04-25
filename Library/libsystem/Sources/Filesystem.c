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


errno_t Mount(MountType type, const char* _Nonnull containerPath, const char* _Nonnull atDirPath, const char* _Nonnull params)
{
    return (errno_t)_syscall(SC_mount, type, containerPath, atDirPath, params);
}

errno_t Unmount(const char* _Nonnull atDirPath, UnmountOptions options)
{
    return (errno_t)_syscall(SC_unmount, atDirPath, options);
}

errno_t s_fsgetdisk(fsid_t fsid, char* _Nonnull buf, size_t bufSize)
{
    return (errno_t)_syscall(SC_fsgetdisk, fsid, buf, bufSize);
}
