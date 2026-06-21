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


errno_t InodeHandler_Create(InodeRef _Nonnull pNode, fd_flags_t oflags, HandlerRef _Nullable * _Nonnull pOutFile)
{
    decl_try_err();
    InodeHandlerRef self;
    
    try(Handler_Create(&kInodeHandlerClass, FD_TYPE_INODE, oflags, (HandlerRef*)&self));
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

errno_t InodeHandler_read(InodeHandlerRef _Nonnull _Locked self, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
{
    decl_try_err();
    const fd_flags_t flags = Handler_GetFlags(self);

    if ((flags & O_RDONLY) == 0) {
        *nOutBytesRead = 0;
        return EBADF;
    }


    Inode_Lock(self->ino);
    err = Inode_Read(self->ino, &self->offset, pBuffer, nBytesToRead, nOutBytesRead);
    Inode_Unlock(self->ino);

    return err;
}

errno_t InodeHandler_write(InodeHandlerRef _Nonnull _Locked self, const void* _Nonnull pBuffer, ssize_t nBytesToWrite, ssize_t* _Nonnull nOutBytesWritten)
{
    decl_try_err();
    const fd_flags_t flags = Handler_GetFlags(self);
    off_t offset;

    if ((flags & O_WRONLY) == 0) {
        *nOutBytesWritten = 0;
        return EBADF;
    }


    Inode_Lock(self->ino);

    if ((flags & O_APPEND) == O_APPEND) {
        offset = Inode_GetFileSize(self);
    }
    else {
        offset = self->offset;
    }

    err = Inode_Write(self->ino, &offset, pBuffer, nBytesToWrite, nOutBytesWritten);

    if (err == EOK) {
        self->offset = offset;
    }

    Inode_Unlock(self->ino);

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

    err = do_seek(offset, whence, endPos, &self->offset);

    if (pOutNewPos && err == EOK) {
        *pOutNewPos = self->offset;
    }
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
    const fd_flags_t flags = Handler_GetFlags(self);

    if (length < 0ll) {
        return EINVAL;
    }
    
    if ((flags & O_WRONLY) == 0) {
        return EBADF;
    }


    // Does not update the file offset
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
