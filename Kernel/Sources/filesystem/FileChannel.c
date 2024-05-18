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

    try(IOChannel_AbstractCreate(&kFileChannelClass, mode, (IOChannelRef*)&self));
    Lock_Init(&self->lock);
    self->inode = pNode;
    self->offset = 0ll;

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

// Creates an independent copy of teh receiver. The newly created file channel
// receives its own independent strong file system and inode references.
errno_t FileChannel_copy(FileChannelRef _Nonnull self, IOChannelRef _Nullable * _Nonnull pOutFile)
{
    decl_try_err();
    FileChannelRef pNewFile = NULL;

    try(IOChannel_AbstractCreate(classof(self), IOChannel_GetMode(self), (IOChannelRef*)&pNewFile));
    Lock_Init(&pNewFile->lock);
    pNewFile->inode = Inode_Reacquire(self->inode);
        
    Lock_Lock(&self->lock);
    pNewFile->offset = self->offset;
    Lock_Unlock(&self->lock);

catch:
    *pOutFile = (IOChannelRef)pNewFile;

    return err;
}

errno_t FileChannel_ioctl(FileChannelRef _Nonnull self, int cmd, va_list ap)
{
    switch (cmd) {
        case kIOChannelCommand_GetType:
            *((int*) va_arg(ap, int*)) = kIOChannelType_File;
            return EOK;

        default:
            return super_n(ioctl, IOChannel, self, cmd, ap);
    }
}

errno_t FileChannel_read(FileChannelRef _Nonnull self, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
{
    decl_try_err();

    Lock_Lock(&self->lock);
    err = Filesystem_ReadFile(Inode_GetFilesystem(self->inode), self->inode, pBuffer, nBytesToRead, &self->offset, nOutBytesRead);
    Lock_Unlock(&self->lock);

    return err;
}

errno_t FileChannel_write(FileChannelRef _Nonnull self, const void* _Nonnull pBuffer, ssize_t nBytesToWrite, ssize_t* _Nonnull nOutBytesWritten)
{
    decl_try_err();
    FileOffset offset;

    Lock_Lock(&self->lock);
    if ((IOChannel_GetMode(self) & kOpen_Append) == kOpen_Append) {
        offset = Inode_GetFileSize(self->inode);
    }
    else {
        offset = self->offset;
    }

    err = Filesystem_WriteFile(Inode_GetFilesystem(self->inode), self->inode, pBuffer, nBytesToWrite, &offset, nOutBytesWritten);
    self->offset = offset;
    Lock_Unlock(&self->lock);

    return err;
}

errno_t FileChannel_seek(FileChannelRef _Nonnull self, FileOffset offset, FileOffset* _Nullable pOutOldPosition, int whence)
{
    decl_try_err();
    FileOffset newOffset;

    Lock_Lock(&self->lock);
    switch (whence) {
        case kSeek_Set:
            if (offset < 0ll) {
                throw(EINVAL);
            }
            newOffset = offset;
            break;

        case kSeek_Current:
            if (offset < 0ll && -offset > self->offset) {
                throw(EINVAL);
            }
            newOffset = self->offset + offset;
            break;

        case kSeek_End:
            if (offset < 0ll && -offset > Inode_GetFileSize(self->inode)) {
                throw(EINVAL);
            }
            newOffset = Inode_GetFileSize(self->inode) + offset;
            break;

        default:
            throw(EINVAL);
    }

    if (newOffset < 0 || newOffset > kFileOffset_Max) {
        throw(EOVERFLOW);
    }

    if(pOutOldPosition) {
        *pOutOldPosition = self->offset;
    }
    self->offset = newOffset;

catch:
    Lock_Unlock(&self->lock);

    return err;
}

errno_t FileChannel_GetInfo(FileChannelRef _Nonnull self, FileInfo* _Nonnull pOutInfo)
{
    return Filesystem_GetFileInfo(Inode_GetFilesystem(self->inode), self->inode, pOutInfo);
}

errno_t FileChannel_SetInfo(FileChannelRef _Nonnull self, User user, MutableFileInfo* _Nonnull pInfo)
{
    return Filesystem_SetFileInfo(Inode_GetFilesystem(self->inode), self->inode, user, pInfo);
}

errno_t FileChannel_Truncate(FileChannelRef _Nonnull self, User user, FileOffset length)
{
    // Does not adjust the file offset
    return Filesystem_Truncate(Inode_GetFilesystem(self->inode), self->inode, user, length);
}


class_func_defs(FileChannel, IOChannel,
override_func_def(finalize, FileChannel, IOChannel)
override_func_def(copy, FileChannel, IOChannel)
override_func_def(ioctl, FileChannel, IOChannel)
override_func_def(read, FileChannel, IOChannel)
override_func_def(write, FileChannel, IOChannel)
override_func_def(seek, FileChannel, IOChannel)
);
