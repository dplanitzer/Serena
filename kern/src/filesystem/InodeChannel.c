//
//  InodeChannel.c
//  kernel
//
//  Created by Dietmar Planitzer on 3/31/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "InodeChannel.h"
#include "Filesystem.h"

#define _get_inode() \
IOChannel_GetResourceAs(self, Inode)


errno_t InodeChannel_Create(InodeRef _Nonnull pNode, unsigned int mode, IOChannelRef _Nullable * _Nonnull pOutFile)
{
    decl_try_err();
    InodeChannelRef self;
    
    try(IOChannel_Create(&kInodeChannelClass, SEO_FT_INODE, mode, (intptr_t)pNode, (IOChannelRef*)&self));
    Inode_Reacquire(pNode);

catch:
    *pOutFile = (IOChannelRef)self;
    return err;
}

errno_t InodeChannel_finalize(InodeChannelRef _Nonnull self)
{
    return Inode_Relinquish(_get_inode());
}

errno_t InodeChannel_read(InodeChannelRef _Nonnull _Locked self, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
{
    decl_try_err();
    InodeRef pn = _get_inode();

    if (IOChannel_IsReadable(self)) {
        Inode_Lock(pn);
        err = Inode_Read(pn, self, pBuffer, nBytesToRead, nOutBytesRead);
        Inode_Unlock(pn);
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
    InodeRef pn = _get_inode();

    if (IOChannel_IsWritable(self)) {
        Inode_Lock(pn);
        err = Inode_Write(pn, self, pBuffer, nBytesToWrite, nOutBytesWritten);
        Inode_Unlock(pn);
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
    InodeRef pn = _get_inode();

    Inode_Lock(pn);
    err = IOChannel_DoSeek((IOChannelRef)self, offset, pOutNewPos, whence);
    Inode_Unlock(pn);

    return err;
}

off_t InodeChannel_getSeekableRange(InodeChannelRef _Nonnull _Locked self)
{
    return Inode_GetFileSize(_get_inode());
}

errno_t InodeChannel_getFileInfo(InodeChannelRef _Nonnull self, struct stat* _Nonnull pOutInfo)
{
    InodeRef pn = _get_inode();

    Inode_Lock(pn);
    Inode_GetInfo(pn, pOutInfo);
    Inode_Unlock(pn);
    
    return EOK;
}

errno_t InodeChannel_truncate(InodeChannelRef _Nonnull self, off_t length)
{
    decl_try_err();
    InodeRef pn = _get_inode();

    if (length < 0ll) {
        return EINVAL;
    }
    
    // Does not adjust the file offset
    Inode_Lock(pn);
    if (S_ISREG(Inode_GetMode(pn))) {
        err = Inode_Truncate(pn, length);
    }
    else {
        err = EBADF;
    }
    Inode_Unlock(pn);
    
    return err;
}


class_func_defs(InodeChannel, IOChannel,
override_func_def(finalize, InodeChannel, IOChannel)
override_func_def(read, InodeChannel, IOChannel)
override_func_def(write, InodeChannel, IOChannel)
override_func_def(seek, InodeChannel, IOChannel)
override_func_def(getSeekableRange, InodeChannel, IOChannel)
override_func_def(getFileInfo, InodeChannel, IOChannel)
override_func_def(truncate, InodeChannel, IOChannel)
);
