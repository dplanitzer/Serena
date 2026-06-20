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


errno_t KfsSpecial_Create(KernFSRef _Nonnull kfs, ino_t inid, fs_perms_t fsperms, uid_t uid, gid_t gid, ino_t pnid, ObjectRef _Nonnull obj, intptr_t arg, KfsNodeRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    nanotime_t now;
    KfsSpecialRef self;

    FSGetCurrentTime(&now);
    
    try(Inode_Create(
        class(KfsSpecial),
        (FilesystemRef)kfs,
        inid,
        FS_FTYPE_DEV,
        fsperms,
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

errno_t KfsSpecial_createHandler(KfsSpecialRef _Nonnull _Locked self, unsigned int mode, HandlerRef _Nullable * _Nonnull pOutHandler)
{
    switch (Inode_GetFileType(self)) {
        case FS_FTYPE_DEV:
            return Driver_Open((DriverRef)self->instance, mode, self->arg, pOutHandler);

        default:
            return EBADF;
    }
}

ObjectRef _Nullable KfsSpecial_getResource(KfsSpecialRef _Nonnull _Locked self)
{
    return self->instance;
}

errno_t KfsSpecial_read(KfsSpecialRef _Nonnull _Locked self, InodeHandlerRef _Nonnull _Locked hnd, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
{
    return EPERM;
}

errno_t KfsSpecial_write(KfsSpecialRef _Nonnull _Locked self, InodeHandlerRef _Nonnull _Locked hnd, const void* _Nonnull pBuffer, ssize_t nBytesToWrite, ssize_t* _Nonnull nOutBytesWritten)
{
    return EPERM;
}

errno_t KfsSpecial_truncate(KfsSpecialRef _Nonnull _Locked self, off_t length)
{
    return EPERM;
}


class_func_defs(KfsSpecial, KfsNode,
override_func_def(deinit, KfsSpecial, Inode)
override_func_def(createHandler, KfsSpecial, Inode)
override_func_def(getResource, KfsSpecial, Inode)
override_func_def(read, KfsSpecial, Inode)
override_func_def(write, KfsSpecial, Inode)
override_func_def(truncate, KfsSpecial, Inode)
);
