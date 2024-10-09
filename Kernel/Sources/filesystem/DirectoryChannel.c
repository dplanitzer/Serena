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

    try(IOChannel_AbstractCreate(&kDirectoryChannelClass, kOpen_Read, (IOChannelRef*)&self));
    Lock_Init(&self->lock);
    self->inode = pDir;
    self->offset = 0ll;

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

// Creates an independent copy of the receiver. The copy receives its own strong
// filesystem and inode references.
ssize_t DirectoryChannel_copy(DirectoryChannelRef _Nonnull self, IOChannelRef _Nullable * _Nonnull pOutDir)
{
    decl_try_err();
    DirectoryChannelRef pNewDir = NULL;

    try(IOChannel_AbstractCreate(classof(self), IOChannel_GetMode(self), (IOChannelRef*)&pNewDir));
    Lock_Init(&pNewDir->lock);
    pNewDir->inode = Inode_Reacquire(self->inode);
    
    Lock_Lock(&self->lock);
    pNewDir->offset = self->offset;
    Lock_Unlock(&self->lock);

catch:
    *pOutDir = (IOChannelRef)pNewDir;

    return err;
}

errno_t DirectoryChannel_ioctl(DirectoryChannelRef _Nonnull self, int cmd, va_list ap)
{
    switch (cmd) {
        case kIOChannelCommand_GetType:
            *((int*) va_arg(ap, int*)) = kIOChannelType_Directory;
            return EOK;

        default:
            return super_n(ioctl, IOChannel, DirectoryChannel, self, cmd, ap);
    }
}

errno_t DirectoryChannel_read(DirectoryChannelRef _Nonnull self, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
{
    decl_try_err();

    Lock_Lock(&self->lock);
    Inode_Lock(self->inode);
    err = Filesystem_ReadDirectory(Inode_GetFilesystem(self->inode), self->inode, pBuffer, nBytesToRead, &self->offset, nOutBytesRead);
    Inode_Unlock(self->inode);
    Lock_Unlock(&self->lock);

    return err;
}

errno_t DirectoryChannel_seek(DirectoryChannelRef _Nonnull self, FileOffset offset, FileOffset* _Nullable pOutOldPosition, int whence)
{
    decl_try_err();

    if (whence != kSeek_Set || offset < 0) {
        return EINVAL;
    }
    if (offset > kFileOffset_Max) {
        return EOVERFLOW;
    }

    Lock_Lock(&self->lock);
    if(pOutOldPosition) {
        *pOutOldPosition = self->offset;
    }
    self->offset = offset;
    Lock_Unlock(&self->lock);

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
override_func_def(copy, DirectoryChannel, IOChannel)
override_func_def(ioctl, DirectoryChannel, IOChannel)
override_func_def(read, DirectoryChannel, IOChannel)
override_func_def(seek, DirectoryChannel, IOChannel)
);
