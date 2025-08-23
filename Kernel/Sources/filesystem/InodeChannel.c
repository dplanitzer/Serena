//
//  InodeChannel.c
//  kernel
//
//  Created by Dietmar Planitzer on 3/31/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "InodeChannel.h"
#include "Filesystem.h"


// InodeChannel uses the Inode lock to protect its seek state


// Creates a file channel.
errno_t InodeChannel_Create(InodeRef _Nonnull pNode, unsigned int mode, IOChannelRef _Nullable * _Nonnull pOutFile)
{
    decl_try_err();
    InodeChannelRef self;
    const int ty = (S_ISDIR(Inode_GetMode(pNode))) ? SEO_FT_DIRECTORY : SEO_FT_REGULAR;
    
    try(IOChannel_Create(&kInodeChannelClass, kIOChannel_Seekable, ty, mode, (IOChannelRef*)&self));
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

void InodeChannel_lock(InodeChannelRef _Nonnull self)
{
    Inode_Lock(self->inode);
}

void InodeChannel_unlock(InodeChannelRef _Nonnull _Locked self)
{
    Inode_Unlock(self->inode);
}

errno_t InodeChannel_read(InodeChannelRef _Nonnull _Locked self, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
{
    return Inode_Read(self->inode, self, pBuffer, nBytesToRead, nOutBytesRead);
}

errno_t InodeChannel_write(InodeChannelRef _Nonnull _Locked self, const void* _Nonnull pBuffer, ssize_t nBytesToWrite, ssize_t* _Nonnull nOutBytesWritten)
{
    return Inode_Write(self->inode, self, pBuffer, nBytesToWrite, nOutBytesWritten);
}


errno_t InodeChannel_seek(InodeChannelRef _Nonnull _Locked self, off_t offset, off_t* _Nullable pOutNewPos, int whence)
{
    decl_try_err();

    if (S_ISREG(Inode_GetMode(self->inode)) || (S_ISDIR(Inode_GetMode(self->inode)) && whence == SEEK_SET)) {
        err = super_n(seek, IOChannel, InodeChannel, self, offset, pOutNewPos, whence);
    }
    else {
        err = EINVAL;
    }

    return err;
}

off_t InodeChannel_getSeekableRange(InodeChannelRef _Nonnull _Locked self)
{
    return Inode_GetFileSize(self->inode);
}

off_t InodeChannel_GetFileSize(InodeChannelRef _Nonnull self)
{
    IOChannel_Lock((IOChannelRef)self);
    const off_t fileSize = IOChannel_GetSeekableRange(self);
    IOChannel_Unlock((IOChannelRef)self);

    return fileSize;
}

void InodeChannel_GetInfo(InodeChannelRef _Nonnull self, struct stat* _Nonnull pOutInfo)
{
    IOChannel_Lock((IOChannelRef)self);
    Inode_GetInfo(self->inode, pOutInfo);
    IOChannel_Unlock((IOChannelRef)self);
}

errno_t InodeChannel_Truncate(InodeChannelRef _Nonnull self, off_t length)
{
    decl_try_err();

    if (length < 0ll) {
        return EINVAL;
    }
    
    // Does not adjust the file offset
    IOChannel_Lock((IOChannelRef)self);
    if (S_ISREG(Inode_GetMode(self->inode))) {
        err = Inode_Truncate(self->inode, length);
    }
    else {
        err = EBADF;
    }
    IOChannel_Unlock((IOChannelRef)self);
    
    return err;
}


class_func_defs(InodeChannel, IOChannel,
override_func_def(finalize, InodeChannel, IOChannel)
override_func_def(lock, InodeChannel, IOChannel)
override_func_def(unlock, InodeChannel, IOChannel)
override_func_def(read, InodeChannel, IOChannel)
override_func_def(write, InodeChannel, IOChannel)
override_func_def(seek, InodeChannel, IOChannel)
override_func_def(getSeekableRange, InodeChannel, IOChannel)
);
