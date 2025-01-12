//
//  DirectoryChannel.c
//  kernel
//
//  Created by Dietmar Planitzer on 3/31/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "DirectoryChannel.h"
#include "Filesystem.h"

// DirectoryChannel uses the Inode lock to protect its seek state


// Creates a directory channel which takes ownership of the provided inode
// reference. This reference will be released by deinit().
errno_t DirectoryChannel_Create(InodeRef _Consuming _Nonnull pDir, IOChannelRef _Nullable * _Nonnull pOutDir)
{
    decl_try_err();
    DirectoryChannelRef self;

    try(IOChannel_Create(&kDirectoryChannelClass, kIOChannel_Seekable, kIOChannelType_Directory, kOpen_Read, (IOChannelRef*)&self));
    self->inode = pDir;

catch:
    *pOutDir = (IOChannelRef)self;
    return err;
}

errno_t DirectoryChannel_finalize(DirectoryChannelRef _Nonnull self)
{
    decl_try_err();

    err = Inode_Relinquish(self->inode);
    self->inode = NULL;

    return err;
}

void DirectoryChannel_lock(DirectoryChannelRef _Nonnull self)
{
    Inode_Lock(self->inode);
}

void DirectoryChannel_unlock(DirectoryChannelRef _Nonnull _Locked self)
{
    Inode_Unlock(self->inode);
}

errno_t DirectoryChannel_read(DirectoryChannelRef _Nonnull _Locked self, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
{
    return Filesystem_ReadDirectory(Inode_GetFilesystem(self->inode), self, pBuffer, nBytesToRead, nOutBytesRead);
}

errno_t DirectoryChannel_seek(DirectoryChannelRef _Nonnull _Locked self, FileOffset offset, FileOffset* _Nullable pOutOldPosition, int whence)
{
    decl_try_err();

    if (whence == kSeek_Set) {
        err = super_n(seek, IOChannel, DirectoryChannel, self, offset, pOutOldPosition, whence);
    }
    else {
        err = EINVAL;
    }

    return err;
}

errno_t DirectoryChannel_GetInfo(DirectoryChannelRef _Nonnull self, FileInfo* _Nonnull pOutInfo)
{
    IOChannel_Lock(self);
    const errno_t err = Filesystem_GetFileInfo(Inode_GetFilesystem(self->inode), self->inode, pOutInfo);
    IOChannel_Unlock(self);
    
    return err;
}

errno_t DirectoryChannel_SetInfo(DirectoryChannelRef _Nonnull self, UserId uid, GroupId gid, MutableFileInfo* _Nonnull pInfo)
{
    IOChannel_Lock(self);
    const errno_t err = Filesystem_SetFileInfo(Inode_GetFilesystem(self->inode), self->inode, uid, gid, pInfo);
    IOChannel_Unlock(self);

    return err;
}


class_func_defs(DirectoryChannel, IOChannel,
override_func_def(finalize, DirectoryChannel, IOChannel)
override_func_def(lock, DirectoryChannel, IOChannel)
override_func_def(unlock, DirectoryChannel, IOChannel)
override_func_def(read, DirectoryChannel, IOChannel)
override_func_def(seek, DirectoryChannel, IOChannel)
);
