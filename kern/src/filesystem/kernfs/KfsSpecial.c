//
//  KfsSpecial.c
//  kernel
//
//  Created by Dietmar Planitzer on 8/3/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "KfsSpecial.h"
#include <filesystem/Filesystem.h>
#include <filesystem/FSUtilities.h>


errno_t KfsSpecial_Create(KernFSRef _Nonnull kfs, ino_t inid, ino_t pnid, const KfsHandlerNodeArgs* args, KfsNodeRef _Nullable * _Nonnull pOutSelf)
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
        args->perms,
        args->uid,
        args->gid,
        1,
        8,
        &now,
        &now,
        &now,
        pnid,
        (InodeRef*)&self));

    if (args->resource) {
        self->resource = Object_Retain(args->resource);
    }
    self->context = args->context;
    self->createHandlerFunc = args->func;

catch:
    *pOutSelf = (KfsNodeRef)self;
    return err;
}

void KfsSpecial_deinit(KfsSpecialRef _Nullable self)
{
    if (self->resource) {
        Object_Release(self->resource);
        self->resource = NULL;
    }
    self->context = NULL;
    self->createHandlerFunc = NULL;
}

errno_t KfsSpecial_createHandler(KfsSpecialRef _Nonnull _Locked self, fd_flags_t flags, HandlerRef _Nullable * _Nonnull pOutHandler)
{
    if (self->createHandlerFunc) {
        return self->createHandlerFunc(self->context, flags, pOutHandler);
    }
    else {
        return EBADF;
    }
}

ObjectRef _Nullable KfsSpecial_getResource(KfsSpecialRef _Nonnull _Locked self)
{
    return self->resource;
}

errno_t KfsSpecial_read(KfsSpecialRef _Nonnull _Locked self, InodeHandlerRef _Nonnull _Locked hnd, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
{
    return EBADF;
}

errno_t KfsSpecial_write(KfsSpecialRef _Nonnull _Locked self, InodeHandlerRef _Nonnull _Locked hnd, const void* _Nonnull pBuffer, ssize_t nBytesToWrite, ssize_t* _Nonnull nOutBytesWritten)
{
    return EBADF;
}

errno_t KfsSpecial_truncate(KfsSpecialRef _Nonnull _Locked self, off_t length)
{
    return EBADF;
}


class_func_defs(KfsSpecial, KfsNode,
override_func_def(deinit, KfsSpecial, Inode)
override_func_def(createHandler, KfsSpecial, Inode)
override_func_def(getResource, KfsSpecial, Inode)
override_func_def(read, KfsSpecial, Inode)
override_func_def(write, KfsSpecial, Inode)
override_func_def(truncate, KfsSpecial, Inode)
);
