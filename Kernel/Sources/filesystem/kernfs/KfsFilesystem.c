//
//  KfsFilesystem.c
//  kernel
//
//  Created by Dietmar Planitzer on 4/12/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "KfsFilesystem.h"
#include <filesystem/FSUtilities.h>


errno_t KfsFilesystem_Create(KernFSRef _Nonnull kfs, ino_t inid, FilePermissions permissions, uid_t uid, gid_t gid, ino_t pnid, FilesystemRef _Nonnull fs, KfsNodeRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    const TimeInterval curTime = FSGetCurrentTime();
    KfsFilesystemRef self;

    try(Inode_Create(
        class(KfsFilesystem),
        (FilesystemRef)kfs,
        inid,
        S_IFFS,
        1,
        uid,
        gid,
        permissions,
        8,
        curTime,
        curTime,
        curTime,
        pnid,
        (InodeRef*)&self));
    self->instance = Object_RetainAs(fs, Filesystem);

catch:
    *pOutSelf = (KfsNodeRef)self;
    return err;
}

void KfsFilesystem_deinit(KfsFilesystemRef _Nullable self)
{
    Object_Release(self->instance);
    self->instance = NULL;
}

errno_t KfsFilesystem_createChannel(KfsFilesystemRef _Nonnull _Locked self, unsigned int mode, IOChannelRef _Nullable * _Nonnull pOutChannel)
{
    return Filesystem_Open(self->instance, mode, 0, pOutChannel);
}

errno_t KfsFilesystem_read(KfsFilesystemRef _Nonnull _Locked self, FileChannelRef _Nonnull _Locked pChannel, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
{
    return EPERM;
}

errno_t KfsFilesystem_write(KfsFilesystemRef _Nonnull _Locked self, FileChannelRef _Nonnull _Locked pChannel, const void* _Nonnull pBuffer, ssize_t nBytesToWrite, ssize_t* _Nonnull nOutBytesWritten)
{
    return EPERM;
}

errno_t KfsFilesystem_truncate(KfsFilesystemRef _Nonnull _Locked self, off_t length)
{
    return EPERM;
}


class_func_defs(KfsFilesystem, KfsNode,
override_func_def(deinit, KfsFilesystem, Inode)
override_func_def(createChannel, KfsFilesystem, Inode)
override_func_def(read, KfsFilesystem, Inode)
override_func_def(write, KfsFilesystem, Inode)
override_func_def(truncate, KfsFilesystem, Inode)
);
