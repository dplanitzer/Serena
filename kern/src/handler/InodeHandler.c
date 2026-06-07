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

#define _get_inode() \
Handler_GetResourceAs(self, Inode)


errno_t InodeHandler_Create(InodeRef _Nonnull pNode, unsigned int mode, HandlerRef _Nullable * _Nonnull pOutFile)
{
    decl_try_err();
    InodeHandlerRef self;
    
    try(Handler_Create(&kInodeHandlerClass, FD_TYPE_INODE, mode, (intptr_t)pNode, (HandlerRef*)&self));
    Inode_Reacquire(pNode);

catch:
    *pOutFile = (HandlerRef)self;
    return err;
}

errno_t InodeHandler_finalize(InodeHandlerRef _Nonnull self)
{
    return Inode_Relinquish(_get_inode());
}

errno_t InodeHandler_read(InodeHandlerRef _Nonnull _Locked self, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
{
    decl_try_err();
    InodeRef pn = _get_inode();

    if (Handler_IsReadable(self)) {
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

errno_t InodeHandler_write(InodeHandlerRef _Nonnull _Locked self, const void* _Nonnull pBuffer, ssize_t nBytesToWrite, ssize_t* _Nonnull nOutBytesWritten)
{
    decl_try_err();
    InodeRef pn = _get_inode();

    if (Handler_IsWritable(self)) {
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


errno_t InodeHandler_seek(InodeHandlerRef _Nonnull _Locked self, off_t offset, off_t* _Nullable pOutNewPos, int whence)
{
    decl_try_err();
    InodeRef pn = _get_inode();

    Inode_Lock(pn);
    err = Handler_DoSeek((HandlerRef)self, offset, pOutNewPos, whence);
    Inode_Unlock(pn);

    return err;
}

off_t InodeHandler_getSeekableRange(InodeHandlerRef _Nonnull _Locked self)
{
    return Inode_GetFileSize(_get_inode());
}

errno_t InodeHandler_getAttributes(InodeHandlerRef _Nonnull self, fs_attr_t* _Nonnull attr)
{
    InodeRef pn = _get_inode();

    Inode_Lock(pn);
    Inode_GetAttributes(pn, attr);
    Inode_Unlock(pn);
    
    return EOK;
}

errno_t InodeHandler_truncate(InodeHandlerRef _Nonnull self, off_t length)
{
    decl_try_err();
    InodeRef pn = _get_inode();

    if (length < 0ll) {
        return EINVAL;
    }
    
    // Does not adjust the file offset
    Inode_Lock(pn);
    if (Inode_IsRegularFile(pn)) {
        err = Inode_Truncate(pn, length);
    }
    else {
        err = EBADF;
    }
    Inode_Unlock(pn);
    
    return err;
}


class_func_defs(InodeHandler, Handler,
override_func_def(finalize, InodeHandler, Handler)
override_func_def(read, InodeHandler, Handler)
override_func_def(write, InodeHandler, Handler)
override_func_def(seek, InodeHandler, Handler)
override_func_def(getSeekableRange, InodeHandler, Handler)
override_func_def(getAttributes, InodeHandler, Handler)
override_func_def(truncate, InodeHandler, Handler)
);
