//
//  KfsSpecial.c
//  kernel
//
//  Created by Dietmar Planitzer on 8/3/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "KfsSpecial.h"
#include <driver/Driver.h>
#include <driver/Driver.h>
#include <filesystem/Filesystem.h>
#include <filesystem/FSUtilities.h>


errno_t KfsSpecial_Create(KernFSRef _Nonnull kfs, ino_t inid, mode_t mode, uid_t uid, gid_t gid, ino_t pnid, ObjectRef _Nonnull obj, intptr_t arg, KfsNodeRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    struct timespec now;
    KfsSpecialRef self;

    FSGetCurrentTime(&now);
    
    try(Inode_Create(
        class(KfsSpecial),
        (FilesystemRef)kfs,
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
    self->instance = Object_Retain(obj);
    self->arg = arg;

catch:
    *pOutSelf = (KfsNodeRef)self;
    return err;
}

void KfsSpecial_deinit(KfsSpecialRef _Nullable self)
{
    Object_Release(self->instance);
    self->instance = NULL;
}

errno_t KfsSpecial_createChannel(KfsSpecialRef _Nonnull _Locked self, unsigned int mode, IOChannelRef _Nullable * _Nonnull pOutChannel)
{
    switch (Inode_GetType(self)) {
        case S_IFDEV:
            return Driver_Open((DriverRef)self->instance, mode, self->arg, pOutChannel);

        case S_IFFS:
            return Filesystem_Open((FilesystemRef)self->instance, mode, 0, pOutChannel);

        default:
            return EBADF;
    }
}

errno_t KfsSpecial_read(KfsSpecialRef _Nonnull _Locked self, InodeChannelRef _Nonnull _Locked pChannel, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
{
    return EPERM;
}

errno_t KfsSpecial_write(KfsSpecialRef _Nonnull _Locked self, InodeChannelRef _Nonnull _Locked pChannel, const void* _Nonnull pBuffer, ssize_t nBytesToWrite, ssize_t* _Nonnull nOutBytesWritten)
{
    return EPERM;
}

errno_t KfsSpecial_truncate(KfsSpecialRef _Nonnull _Locked self, off_t length)
{
    return EPERM;
}


class_func_defs(KfsSpecial, KfsNode,
override_func_def(deinit, KfsSpecial, Inode)
override_func_def(createChannel, KfsSpecial, Inode)
override_func_def(read, KfsSpecial, Inode)
override_func_def(write, KfsSpecial, Inode)
override_func_def(truncate, KfsSpecial, Inode)
);
