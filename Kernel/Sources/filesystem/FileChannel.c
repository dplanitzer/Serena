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
    self->filesystem = pFilesystem;
    self->inode = pNode;
    self->offset = 0ll;

catch:
    *pOutFile = (IOChannelRef)self;
    return err;
}

void FileChannel_deinit(FileChannelRef _Nonnull self)
{
    if (self->inode) {
        Filesystem_RelinquishNode((FilesystemRef)self->filesystem, self->inode);
        self->inode = NULL;
    }
    if (self->filesystem) {
        Object_Release(self->filesystem);
        self->filesystem = NULL;
    }
}

// Creates an independent copy of teh receiver. The newly created file channel
// receives its own independent strong file system and inode references.
errno_t FileChannel_dup(FileChannelRef _Nonnull self, IOChannelRef _Nullable * _Nonnull pOutFile)
{
    decl_try_err();
    FileChannelRef pNewFile;

    try(IOChannel_AbstractCreateCopy((IOChannelRef)self, (IOChannelRef*)&pNewFile));
    pNewFile->filesystem = Object_Retain(self->filesystem);
    pNewFile->inode = Filesystem_ReacquireUnlockedNode((FilesystemRef)self->filesystem, self->inode);
    pNewFile->offset = self->offset;

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
            return Object_SuperN(ioctl, IOChannel, self, cmd, ap);
    }
}

errno_t FileChannel_read(FileChannelRef _Nonnull self, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
{
    return Filesystem_ReadFile((FilesystemRef)self->filesystem, self->inode, pBuffer, nBytesToRead, &self->offset, nOutBytesRead);
}

errno_t FileChannel_write(FileChannelRef _Nonnull self, const void* _Nonnull pBuffer, ssize_t nBytesToWrite, ssize_t* _Nonnull nOutBytesWritten)
{
    decl_try_err();
    FileOffset offset;

    if ((IOChannel_GetMode(self) & kOpen_Append) == kOpen_Append) {
        offset = Inode_GetFileSize(self->inode);
    } else {
        offset = self->offset;
    }

    err = Filesystem_WriteFile((FilesystemRef)self->filesystem, self->inode, pBuffer, nBytesToWrite, &offset, nOutBytesWritten);
    self->offset = offset;

    return err;
}

errno_t FileChannel_seek(FileChannelRef _Nonnull self, FileOffset offset, FileOffset* _Nullable pOutOldPosition, int whence)
{
    if(pOutOldPosition) {
        *pOutOldPosition = self->offset;
    }

    FileOffset newOffset;

    switch (whence) {
        case kSeek_Set:
            newOffset = offset;
            break;

        case kSeek_Current:
            newOffset = self->offset + offset;
            break;

        case kSeek_End:
            newOffset = Inode_GetFileSize(self->inode) + offset;
            break;

        default:
            return EINVAL;
    }

    if (newOffset < 0) {
        return EINVAL;
    }
    // XXX do overflow check

    self->offset = newOffset;
    return EOK;
}

errno_t FileChannel_GetInfo(FileChannelRef _Nonnull self, FileInfo* _Nonnull pOutInfo)
{
    return Filesystem_GetFileInfo((FilesystemRef)self->filesystem, self->inode, pOutInfo);
}

errno_t FileChannel_SetInfo(FileChannelRef _Nonnull self, User user, MutableFileInfo* _Nonnull pInfo)
{
    return Filesystem_SetFileInfo((FilesystemRef)self->filesystem, self->inode, user, pInfo);
}

errno_t FileChannel_Truncate(FileChannelRef _Nonnull self, User user, FileOffset length)
{
    return Filesystem_Truncate((FilesystemRef)self->filesystem, self->inode, user, length);
}


CLASS_METHODS(FileChannel, IOChannel,
OVERRIDE_METHOD_IMPL(deinit, FileChannel, Object)
OVERRIDE_METHOD_IMPL(dup, FileChannel, IOChannel)
OVERRIDE_METHOD_IMPL(ioctl, FileChannel, IOChannel)
OVERRIDE_METHOD_IMPL(read, FileChannel, IOChannel)
OVERRIDE_METHOD_IMPL(write, FileChannel, IOChannel)
OVERRIDE_METHOD_IMPL(seek, FileChannel, IOChannel)
);
