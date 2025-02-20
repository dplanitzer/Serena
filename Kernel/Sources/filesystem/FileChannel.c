//
//  FileChannel.c
//  kernel
//
//  Created by Dietmar Planitzer on 3/31/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "FileChannel.h"
#include "Filesystem.h"

// FileChannel uses the Inode lock to protect its seek state


// Creates a file channel which takes ownership of the provided inode reference.
// This reference will be released by deinit().
errno_t FileChannel_Create(InodeRef _Nonnull pNode, unsigned int mode, IOChannelRef _Nullable * _Nonnull pOutFile)
{
    decl_try_err();
    FileChannelRef self;

    try(IOChannel_Create(&kFileChannelClass, kIOChannel_Seekable, kIOChannelType_File, mode, (IOChannelRef*)&self));
    self->inode = Inode_Reacquire(pNode);

catch:
    *pOutFile = (IOChannelRef)self;
    return err;
}

errno_t FileChannel_finalize(FileChannelRef _Nonnull self)
{
    decl_try_err();

    err = Inode_Relinquish(self->inode);
    self->inode = NULL;

    return err;
}

void FileChannel_lock(FileChannelRef _Nonnull self)
{
    Inode_Lock(self->inode);
}

void FileChannel_unlock(FileChannelRef _Nonnull _Locked self)
{
    Inode_Unlock(self->inode);
}

errno_t FileChannel_read(FileChannelRef _Nonnull _Locked self, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
{
    return Filesystem_ReadFile(Inode_GetFilesystem(self->inode), self, pBuffer, nBytesToRead, nOutBytesRead);
}

errno_t FileChannel_write(FileChannelRef _Nonnull _Locked self, const void* _Nonnull pBuffer, ssize_t nBytesToWrite, ssize_t* _Nonnull nOutBytesWritten)
{
    return Filesystem_WriteFile(Inode_GetFilesystem(self->inode), self, pBuffer, nBytesToWrite, nOutBytesWritten);
}

FileOffset FileChannel_getSeekableRange(FileChannelRef _Nonnull _Locked self)
{
    return Inode_GetFileSize(self->inode);
}

FileOffset FileChannel_GetFileSize(FileChannelRef _Nonnull self)
{
    IOChannel_Lock((IOChannelRef)self);
    const FileOffset fileSize = IOChannel_GetSeekableRange(self);
    IOChannel_Unlock((IOChannelRef)self);

    return fileSize;
}

errno_t FileChannel_GetInfo(FileChannelRef _Nonnull self, FileInfo* _Nonnull pOutInfo)
{
    IOChannel_Lock((IOChannelRef)self);
    const errno_t err = Inode_GetInfo(self->inode, pOutInfo);
    IOChannel_Unlock((IOChannelRef)self);
    
    return err;
}

errno_t FileChannel_SetInfo(FileChannelRef _Nonnull self, UserId uid, GroupId gid, MutableFileInfo* _Nonnull pInfo)
{
    IOChannel_Lock((IOChannelRef)self);
    const errno_t err = Inode_SetInfo(self->inode, uid, gid, pInfo);
    IOChannel_Unlock((IOChannelRef)self);
    
    return err;
}

errno_t FileChannel_Truncate(FileChannelRef _Nonnull self, FileOffset length)
{
    if (length < 0ll) {
        return EINVAL;
    }
    
    // Does not adjust the file offset
    IOChannel_Lock((IOChannelRef)self);
    const errno_t err = Filesystem_TruncateFile(Inode_GetFilesystem(self->inode), self->inode, length);
    IOChannel_Unlock((IOChannelRef)self);
    
    return err;
}


class_func_defs(FileChannel, IOChannel,
override_func_def(finalize, FileChannel, IOChannel)
override_func_def(lock, FileChannel, IOChannel)
override_func_def(unlock, FileChannel, IOChannel)
override_func_def(read, FileChannel, IOChannel)
override_func_def(write, FileChannel, IOChannel)
override_func_def(getSeekableRange, FileChannel, IOChannel)
);
