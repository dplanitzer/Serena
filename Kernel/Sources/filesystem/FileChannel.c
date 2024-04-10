//
//  FileChannel.c
//  kernel
//
//  Created by Dietmar Planitzer on 3/31/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "FileChannel.h"
#include "Filesystem.h"


// Creates a file channel which takes ownership of the provided filesystem and
// inode references. These references will be released by deinit().
errno_t FileChannel_Create(ObjectRef _Consuming _Nonnull pFilesystem, InodeRef _Consuming _Nonnull pNode, unsigned int mode, IOChannelRef _Nullable * _Nonnull pOutFile)
{
    decl_try_err();
    FileChannelRef self;

    try(IOChannel_AbstractCreate(&kFileChannelClass, mode, (IOChannelRef*)&self));
    Lock_Init(&self->lock);
    self->filesystem = pFilesystem;
    self->inode = pNode;
    self->offset = 0ll;

catch:
    *pOutFile = (IOChannelRef)self;
    return err;
}

errno_t FileChannel_close(FileChannelRef _Nonnull self)
{
    Lock_Lock(&self->lock);
    if (self->inode) {
        Filesystem_RelinquishNode((FilesystemRef)self->filesystem, self->inode);
        self->inode = NULL;
    }
    if (self->filesystem) {
        Object_Release(self->filesystem);
        self->filesystem = NULL;
    }
    Lock_Unlock(&self->lock);

    return EOK;
}

void FileChannel_deinit(FileChannelRef _Nonnull self)
{
    if (self->inode) {
        FileChannel_close(self);
    }

    Lock_Deinit(&self->lock);
}

// Creates an independent copy of teh receiver. The newly created file channel
// receives its own independent strong file system and inode references.
errno_t FileChannel_dup(FileChannelRef _Nonnull self, IOChannelRef _Nullable * _Nonnull pOutFile)
{
    decl_try_err();
    FileChannelRef pNewFile = NULL;

    Lock_Lock(&self->lock);
    if (self->inode) {
        try(IOChannel_AbstractCreateCopy((IOChannelRef)self, (IOChannelRef*)&pNewFile));
        Lock_Init(&pNewFile->lock);
        pNewFile->filesystem = Object_Retain(self->filesystem);
        pNewFile->inode = Filesystem_ReacquireUnlockedNode((FilesystemRef)self->filesystem, self->inode);
        pNewFile->offset = self->offset;
    }
    else {
        err = EBADF;
    }

catch:
    Lock_Unlock(&self->lock);
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
            return Object_SuperN(ioctl, IOChannel, self, cmd, ap);
    }
}

errno_t FileChannel_read(FileChannelRef _Nonnull self, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
{
    decl_try_err();

    Lock_Lock(&self->lock);
    if (self->inode) {
        err = Filesystem_ReadFile((FilesystemRef)self->filesystem, self->inode, pBuffer, nBytesToRead, &self->offset, nOutBytesRead);
    }
    else {
        err = EBADF;
    }
    Lock_Unlock(&self->lock);

    return err;
}

errno_t FileChannel_write(FileChannelRef _Nonnull self, const void* _Nonnull pBuffer, ssize_t nBytesToWrite, ssize_t* _Nonnull nOutBytesWritten)
{
    decl_try_err();
    FileOffset offset;

    Lock_Lock(&self->lock);
    if (self->inode) {
        if ((IOChannel_GetMode(self) & kOpen_Append) == kOpen_Append) {
            offset = Inode_GetFileSize(self->inode);
        }
        else {
            offset = self->offset;
        }

        err = Filesystem_WriteFile((FilesystemRef)self->filesystem, self->inode, pBuffer, nBytesToWrite, &offset, nOutBytesWritten);
        self->offset = offset;
    }
    else {
        err = EBADF;
    }
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
    decl_try_err();

    Lock_Lock(&self->lock);
    if (self->inode) {
        err = Filesystem_GetFileInfo((FilesystemRef)self->filesystem, self->inode, pOutInfo);
    }
    else {
        err = EBADF;
    }
    Lock_Unlock(&self->lock);
    
    return err;
}

errno_t FileChannel_SetInfo(FileChannelRef _Nonnull self, User user, MutableFileInfo* _Nonnull pInfo)
{
    decl_try_err();

    Lock_Lock(&self->lock);
    if (self->inode) {
        err = Filesystem_SetFileInfo((FilesystemRef)self->filesystem, self->inode, user, pInfo);
    }
    else {
        err = EBADF;
    }
    Lock_Unlock(&self->lock);
    
    return err;
}

errno_t FileChannel_Truncate(FileChannelRef _Nonnull self, User user, FileOffset length)
{
    decl_try_err();

    Lock_Lock(&self->lock);
    if (self->inode) {
        err = Filesystem_Truncate((FilesystemRef)self->filesystem, self->inode, user, length);
    }
    else {
        err = EBADF;
    }
    Lock_Unlock(&self->lock);
    
    return err;
}


CLASS_METHODS(FileChannel, IOChannel,
OVERRIDE_METHOD_IMPL(deinit, FileChannel, Object)
OVERRIDE_METHOD_IMPL(dup, FileChannel, IOChannel)
OVERRIDE_METHOD_IMPL(close, FileChannel, IOChannel)
OVERRIDE_METHOD_IMPL(ioctl, FileChannel, IOChannel)
OVERRIDE_METHOD_IMPL(read, FileChannel, IOChannel)
OVERRIDE_METHOD_IMPL(write, FileChannel, IOChannel)
OVERRIDE_METHOD_IMPL(seek, FileChannel, IOChannel)
);
