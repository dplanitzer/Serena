//
//  InodeChannel.c
//  kernel
//
//  Created by Dietmar Planitzer on 3/31/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "InodeChannel.h"
#include "Filesystem.h"


// Creates a file channel.
errno_t InodeChannel_Create(InodeRef _Nonnull pNode, unsigned int mode, IOChannelRef _Nullable * _Nonnull pOutFile)
{
    decl_try_err();
    InodeChannelRef self;
    
    try(IOChannel_Create(&kInodeChannelClass, SEO_FT_INODE, mode, (IOChannelRef*)&self));
    self->inode = Inode_Reacquire(pNode);

catch:
    *pOutFile = (IOChannelRef)self;
    return err;
}

errno_t InodeChannel_finalize(InodeChannelRef _Nonnull self)
{
    decl_try_err();

    err = Inode_Relinquish(self->inode);
    self->inode = NULL;

    return err;
}

errno_t InodeChannel_read(InodeChannelRef _Nonnull _Locked self, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
{
    decl_try_err();

    if (IOChannel_IsReadable(self)) {
        Inode_Lock(self->inode);
        err = Inode_Read(self->inode, self, pBuffer, nBytesToRead, nOutBytesRead);
        Inode_Unlock(self->inode);
    }
    else {
        *nOutBytesRead = 0;
        err = EBADF;
    }

    return err;
}

errno_t InodeChannel_write(InodeChannelRef _Nonnull _Locked self, const void* _Nonnull pBuffer, ssize_t nBytesToWrite, ssize_t* _Nonnull nOutBytesWritten)
{
    decl_try_err();

    if (IOChannel_IsWritable(self)) {
        Inode_Lock(self->inode);
        err = Inode_Write(self->inode, self, pBuffer, nBytesToWrite, nOutBytesWritten);
        Inode_Unlock(self->inode);
    }
    else {
        *nOutBytesWritten = 0;
        err = EBADF;
    }

    return err;
}


errno_t InodeChannel_seek(InodeChannelRef _Nonnull _Locked self, off_t offset, off_t* _Nullable pOutNewPos, int whence)
{
    decl_try_err();

    Inode_Lock(self->inode);
    err = IOChannel_DoSeek((IOChannelRef)self, offset, pOutNewPos, whence);
    Inode_Unlock(self->inode);

    return err;
}

off_t InodeChannel_getSeekableRange(InodeChannelRef _Nonnull _Locked self)
{
    return Inode_GetFileSize(self->inode);
}

void InodeChannel_GetInfo(InodeChannelRef _Nonnull self, struct stat* _Nonnull pOutInfo)
{
    Inode_Lock(self->inode);
    Inode_GetInfo(self->inode, pOutInfo);
    Inode_Unlock(self->inode);
}

errno_t InodeChannel_Truncate(InodeChannelRef _Nonnull self, off_t length)
{
    decl_try_err();

    if (length < 0ll) {
        return EINVAL;
    }
    
    // Does not adjust the file offset
    Inode_Lock(self->inode);
    if (S_ISREG(Inode_GetMode(self->inode))) {
        err = Inode_Truncate(self->inode, length);
    }
    else {
        err = EBADF;
    }
    Inode_Unlock(self->inode);
    
    return err;
}


class_func_defs(InodeChannel, IOChannel,
override_func_def(finalize, InodeChannel, IOChannel)
override_func_def(read, InodeChannel, IOChannel)
override_func_def(write, InodeChannel, IOChannel)
override_func_def(seek, InodeChannel, IOChannel)
override_func_def(getSeekableRange, InodeChannel, IOChannel)
);
