//
//  DfsDevice.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/18/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "DfsDevice.h"
#include <filesystem/FSUtilities.h>


errno_t DfsDevice_Create(DevFSRef _Nonnull fs, ino_t inid, FilePermissions permissions, uid_t uid, gid_t gid, DriverRef _Nonnull pDriver, intptr_t arg, DfsNodeRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    const TimeInterval curTime = FSGetCurrentTime();
    DfsDeviceRef self;

    try(Inode_Create(
        class(DfsDevice),
        (FilesystemRef)fs,
        inid,
        kFileType_Device,
        1,
        uid,
        gid,
        permissions,
        8,
        curTime,
        curTime,
        curTime,
        (InodeRef*)&self));
    self->instance = Object_RetainAs(pDriver, Driver);
    self->arg = arg;

catch:
    *pOutSelf = (DfsNodeRef)self;
    return err;
}

void DfsDevice_deinit(DfsDeviceRef _Nullable self)
{
    Object_Release(self->instance);
    self->instance = NULL;
    self->arg = 0;
}

errno_t DfsDevice_createChannel(DfsDeviceRef _Nonnull _Locked self, unsigned int mode, IOChannelRef _Nullable * _Nonnull pOutChannel)
{
    return Driver_Open(self->instance, mode, self->arg, pOutChannel);
}

errno_t DfsDevice_read(DfsDeviceRef _Nonnull _Locked self, FileChannelRef _Nonnull _Locked pChannel, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
{
    return EPERM;
}

errno_t DfsDevice_write(DfsDeviceRef _Nonnull _Locked self, FileChannelRef _Nonnull _Locked pChannel, const void* _Nonnull pBuffer, ssize_t nBytesToWrite, ssize_t* _Nonnull nOutBytesWritten)
{
    return EPERM;
}

errno_t DfsDevice_truncate(DfsDeviceRef _Nonnull _Locked self, off_t length)
{
    return EPERM;
}


class_func_defs(DfsDevice, DfsNode,
override_func_def(deinit, DfsDevice, Inode)
override_func_def(createChannel, DfsDevice, Inode)
override_func_def(read, DfsDevice, Inode)
override_func_def(write, DfsDevice, Inode)
override_func_def(truncate, DfsDevice, Inode)
);
