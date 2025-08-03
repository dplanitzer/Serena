//
//  KfsDevice.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/18/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "KfsDevice.h"
#include <filesystem/FSUtilities.h>


errno_t KfsDevice_Create(KernFSRef _Nonnull fs, ino_t inid, mode_t mode, uid_t uid, gid_t gid, ino_t pnid, DriverRef _Nonnull pDriver, intptr_t arg, KfsNodeRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    struct timespec now;
    KfsDeviceRef self;

    FSGetCurrentTime(&now);
    
    try(Inode_Create(
        class(KfsDevice),
        (FilesystemRef)fs,
        inid,
        mode,
        uid,
        gid,
        1,
        8,
        &now,
        &now,
        &now,
        pnid,
        (InodeRef*)&self));
    self->instance = Object_RetainAs(pDriver, Driver);
    self->arg = arg;

catch:
    *pOutSelf = (KfsNodeRef)self;
    return err;
}

void KfsDevice_deinit(KfsDeviceRef _Nullable self)
{
    Object_Release(self->instance);
    self->instance = NULL;
    self->arg = 0;
}

errno_t KfsDevice_createChannel(KfsDeviceRef _Nonnull _Locked self, unsigned int mode, IOChannelRef _Nullable * _Nonnull pOutChannel)
{
    return Driver_Open(self->instance, mode, self->arg, pOutChannel);
}

errno_t KfsDevice_read(KfsDeviceRef _Nonnull _Locked self, FileChannelRef _Nonnull _Locked pChannel, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
{
    return EPERM;
}

errno_t KfsDevice_write(KfsDeviceRef _Nonnull _Locked self, FileChannelRef _Nonnull _Locked pChannel, const void* _Nonnull pBuffer, ssize_t nBytesToWrite, ssize_t* _Nonnull nOutBytesWritten)
{
    return EPERM;
}

errno_t KfsDevice_truncate(KfsDeviceRef _Nonnull _Locked self, off_t length)
{
    return EPERM;
}


class_func_defs(KfsDevice, KfsNode,
override_func_def(deinit, KfsDevice, Inode)
override_func_def(createChannel, KfsDevice, Inode)
override_func_def(read, KfsDevice, Inode)
override_func_def(write, KfsDevice, Inode)
override_func_def(truncate, KfsDevice, Inode)
);
