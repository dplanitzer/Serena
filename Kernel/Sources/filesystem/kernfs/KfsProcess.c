//
//  KfsProcess.c
//  kernel
//
//  Created by Dietmar Planitzer on 5/10/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "KfsProcess.h"
#include <filesystem/FSUtilities.h>
#include <process/Process.h>


errno_t KfsProcess_Create(KernFSRef _Nonnull kfs, ino_t inid, FilePermissions permissions, uid_t uid, gid_t gid, ino_t pnid, ProcessRef _Nonnull proc, KfsNodeRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    const TimeInterval curTime = FSGetCurrentTime();
    KfsProcessRef self;

    try(Inode_Create(
        class(KfsProcess),
        (FilesystemRef)kfs,
        inid,
        S_IFPROC,
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
    self->instance = Object_RetainAs(proc, Process);

catch:
    *pOutSelf = (KfsNodeRef)self;
    return err;
}

void KfsProcess_deinit(KfsProcessRef _Nullable self)
{
    Object_Release(self->instance);
    self->instance = NULL;
}

errno_t KfsProcess_createChannel(KfsProcessRef _Nonnull _Locked self, unsigned int mode, IOChannelRef _Nullable * _Nonnull pOutChannel)
{
    //XXXreturn Filesystem_Open(self->instance, mode, 0, pOutChannel);
    return ENOTSUP;
}

errno_t KfsProcess_read(KfsProcessRef _Nonnull _Locked self, FileChannelRef _Nonnull _Locked pChannel, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
{
    return EPERM;
}

errno_t KfsProcess_write(KfsProcessRef _Nonnull _Locked self, FileChannelRef _Nonnull _Locked pChannel, const void* _Nonnull pBuffer, ssize_t nBytesToWrite, ssize_t* _Nonnull nOutBytesWritten)
{
    return EPERM;
}

errno_t KfsProcess_truncate(KfsProcessRef _Nonnull _Locked self, off_t length)
{
    return ENOTSUP;
}


class_func_defs(KfsProcess, KfsNode,
override_func_def(deinit, KfsProcess, Inode)
override_func_def(createChannel, KfsProcess, Inode)
override_func_def(read, KfsProcess, Inode)
override_func_def(write, KfsProcess, Inode)
override_func_def(truncate, KfsProcess, Inode)
);
