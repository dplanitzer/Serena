//
//  DirectoryChannel.c
//  kernel
//
//  Created by Dietmar Planitzer on 3/31/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "DirectoryChannel.h"
#include "Filesystem.h"


// Creates a directory channel which takes ownership of the provided inode
// reference. This reference will be released by deinit().
errno_t DirectoryChannel_Create(InodeRef _Consuming _Nonnull pDir, IOChannelRef _Nullable * _Nonnull pOutDir)
{
    decl_try_err();
    DirectoryChannelRef self;

    try(IOChannel_Create(&kDirectoryChannelClass, kIOChannel_Seekable, kIOChannelType_Directory, kOpen_Read, (IOChannelRef*)&self));
    Lock_Init(&self->lock);
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

    Lock_Deinit(&self->lock);
    return err;
}

errno_t DirectoryChannel_read(DirectoryChannelRef _Nonnull self, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
{
    decl_try_err();

    Lock_Lock(&self->lock);
    Inode_Lock(self->inode);
    err = Filesystem_ReadDirectory(Inode_GetFilesystem(self->inode), self, pBuffer, nBytesToRead, nOutBytesRead);
    Inode_Unlock(self->inode);
    Lock_Unlock(&self->lock);

    return err;
}

errno_t DirectoryChannel_seek(DirectoryChannelRef _Nonnull self, FileOffset offset, FileOffset* _Nullable pOutOldPosition, int whence)
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
    Inode_Lock(self->inode);
    const errno_t err = Filesystem_GetFileInfo(Inode_GetFilesystem(self->inode), self->inode, pOutInfo);
    Inode_Unlock(self->inode);
    
    return err;
}

errno_t DirectoryChannel_SetInfo(DirectoryChannelRef _Nonnull self, User user, MutableFileInfo* _Nonnull pInfo)
{
    Inode_Lock(self->inode);
    const errno_t err = Filesystem_SetFileInfo(Inode_GetFilesystem(self->inode), self->inode, user, pInfo);
    Inode_Unlock(self->inode);

    return err;
}


class_func_defs(DirectoryChannel, IOChannel,
override_func_def(finalize, DirectoryChannel, IOChannel)
override_func_def(read, DirectoryChannel, IOChannel)
override_func_def(seek, DirectoryChannel, IOChannel)
);
