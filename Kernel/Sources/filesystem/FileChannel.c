//
//  FileChannel.c
//  kernel
//
//  Created by Dietmar Planitzer on 3/31/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "FileChannel.h"
#include "Filesystem.h"


// Creates a file channel which takes ownership of the provided inode reference.
// This reference will be released by deinit().
errno_t FileChannel_Create(InodeRef _Consuming _Nonnull pNode, unsigned int mode, IOChannelRef _Nullable * _Nonnull pOutFile)
{
    decl_try_err();
    FileChannelRef self;

    try(IOChannel_Create(&kFileChannelClass, kIOChannel_Seekable, kIOChannelType_File, mode, (IOChannelRef*)&self));
    Lock_Init(&self->lock);
    self->inode = pNode;

catch:
    *pOutFile = (IOChannelRef)self;
    return err;
}

errno_t FileChannel_finalize(FileChannelRef _Nonnull self)
{
    decl_try_err();

    err = Inode_Relinquish(self->inode);
    self->inode = NULL;

    Lock_Deinit(&self->lock);
    return err;
}

errno_t FileChannel_read(FileChannelRef _Nonnull self, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
{
    decl_try_err();

    Lock_Lock(&self->lock);
    Inode_Lock(self->inode);
    err = Filesystem_ReadFile(Inode_GetFilesystem(self->inode), self, pBuffer, nBytesToRead, nOutBytesRead);
    Inode_Unlock(self->inode);
    Lock_Unlock(&self->lock);

    return err;
}

errno_t FileChannel_write(FileChannelRef _Nonnull self, const void* _Nonnull pBuffer, ssize_t nBytesToWrite, ssize_t* _Nonnull nOutBytesWritten)
{
    decl_try_err();

    Lock_Lock(&self->lock);
    Inode_Lock(self->inode);
    err = Filesystem_WriteFile(Inode_GetFilesystem(self->inode), self, pBuffer, nBytesToWrite, nOutBytesWritten);
    Inode_Unlock(self->inode);
    Lock_Unlock(&self->lock);

    return err;
}

FileOffset FileChannel_getSeekableRange(FileChannelRef _Nonnull self)
{
    Inode_Lock(self->inode);
    const FileOffset size = Inode_GetFileSize(self->inode);
    Inode_Unlock(self->inode);
    return size;
}

FileOffset FileChannel_GetFileSize(FileChannelRef _Nonnull self)
{
    return IOChannel_GetSeekableRange(self);
}

errno_t FileChannel_GetInfo(FileChannelRef _Nonnull self, FileInfo* _Nonnull pOutInfo)
{
    Inode_Lock(self->inode);
    const errno_t err = Filesystem_GetFileInfo(Inode_GetFilesystem(self->inode), self->inode, pOutInfo);
    Inode_Unlock(self->inode);
    return err;
}

errno_t FileChannel_SetInfo(FileChannelRef _Nonnull self, User user, MutableFileInfo* _Nonnull pInfo)
{
    Inode_Lock(self->inode);
    const errno_t err = Filesystem_SetFileInfo(Inode_GetFilesystem(self->inode), self->inode, user, pInfo);
    Inode_Unlock(self->inode);
    return err;
}

errno_t FileChannel_Truncate(FileChannelRef _Nonnull self, FileOffset length)
{
    if (length < 0ll) {
        return EINVAL;
    }
    
    // Does not adjust the file offset
    Inode_Lock(self->inode);
    const errno_t err = Filesystem_TruncateFile(Inode_GetFilesystem(self->inode), self->inode, length);
    Inode_Unlock(self->inode);
    return err;
}


class_func_defs(FileChannel, IOChannel,
override_func_def(finalize, FileChannel, IOChannel)
override_func_def(read, FileChannel, IOChannel)
override_func_def(write, FileChannel, IOChannel)
override_func_def(getSeekableRange, FileChannel, IOChannel)
);
