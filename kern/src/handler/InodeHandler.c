//
//  InodeHandler.c
//  kernel
//
//  Created by Dietmar Planitzer on 3/31/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "InodeHandler.h"
#include <filesystem/Filesystem.h>
#include <filesystem/Inode.h>
#include <kpi/fd.h>


errno_t InodeHandler_Create(InodeRef _Nonnull pNode, unsigned int mode, HandlerRef _Nullable * _Nonnull pOutFile)
{
    decl_try_err();
    InodeHandlerRef self;
    
    try(Handler_Create(&kInodeHandlerClass, FD_TYPE_INODE, mode, (HandlerRef*)&self));
    self->ino = Inode_Reacquire(pNode);

catch:
    *pOutFile = (HandlerRef)self;
    return err;
}

void InodeHandler_deinit(InodeHandlerRef _Nonnull self)
{
    (void)Inode_Relinquish(self->ino);
    self->ino = NULL;
}

//XXX maybe override shutdown and flush all pending disk blocks for the file
//XXX at least if this is a directory

errno_t InodeHandler_read(InodeHandlerRef _Nonnull _Locked self, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
{
    decl_try_err();

    if (Handler_IsReadable(self)) {
        Inode_Lock(self->ino);
        err = Inode_Read(self->ino, self, pBuffer, nBytesToRead, nOutBytesRead);
        Inode_Unlock(self->ino);
    }
    else {
        *nOutBytesRead = 0;
        err = EBADF;
    }

    return err;
}

errno_t InodeHandler_write(InodeHandlerRef _Nonnull _Locked self, const void* _Nonnull pBuffer, ssize_t nBytesToWrite, ssize_t* _Nonnull nOutBytesWritten)
{
    decl_try_err();

    if (Handler_IsWritable(self)) {
        Inode_Lock(self->ino);
        err = Inode_Write(self->ino, self, pBuffer, nBytesToWrite, nOutBytesWritten);
        Inode_Unlock(self->ino);
    }
    else {
        *nOutBytesWritten = 0;
        err = EBADF;
    }

    return err;
}


errno_t InodeHandler_seek(InodeHandlerRef _Nonnull _Locked self, off_t offset, off_t* _Nullable pOutNewPos, int whence)
{
    decl_try_err();
    off_t endPos = 0ll;

    Inode_Lock(self->ino);
    if (whence == SEEK_END) {
        endPos = Inode_GetFileSize(self->ino);
    }
    err = Handler_DoSeek((HandlerRef)self, offset, endPos, pOutNewPos, whence);
    Inode_Unlock(self->ino);

    return err;
}

errno_t InodeHandler_getAttributes(InodeHandlerRef _Nonnull self, fs_attr_t* _Nonnull attr)
{
    Inode_Lock(self->ino);
    Inode_GetAttributes(self->ino, attr);
    Inode_Unlock(self->ino);
    
    return EOK;
}

errno_t InodeHandler_truncate(InodeHandlerRef _Nonnull self, off_t length)
{
    decl_try_err();

    if (length < 0ll) {
        return EINVAL;
    }
    
    // Does not adjust the file offset
    Inode_Lock(self->ino);
    if (Inode_IsRegularFile(self->ino)) {
        err = Inode_Truncate(self->ino, length);
    }
    else {
        err = EBADF;
    }
    Inode_Unlock(self->ino);
    
    return err;
}


class_func_defs(InodeHandler, Handler,
override_func_def(deinit, InodeHandler, Object)
override_func_def(read, InodeHandler, Handler)
override_func_def(write, InodeHandler, Handler)
override_func_def(seek, InodeHandler, Handler)
func_def(getAttributes, InodeHandler)
func_def(truncate, InodeHandler)
);
