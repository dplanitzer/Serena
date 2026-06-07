//
//  PipeHandler.c
//  kernel
//
//  Created by Dietmar Planitzer on 3/31/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "PipeHandler.h"
#include <ipc/Pipe.h>


#define _get_pipe() \
Handler_GetResourceAs(self, Pipe)


// PipeHandler does not need locking because:
// - it doesn't support seeking. Thus seeking state is constant
// - the pipe implementation protects read/write with a pipe lock anyway
 
errno_t PipeHandler_Create(PipeRef _Nonnull pipe, unsigned int mode, HandlerRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    PipeHandlerRef self;

    if ((mode & O_RDWR) == 0 || (mode & O_RDWR) == O_RDWR) {
        return EACCESS;
    }

    err = Handler_Create(&kPipeHandlerClass, FD_TYPE_INODE, mode, (intptr_t)pipe, (HandlerRef*)&self);
    if (err == EOK) {
        Object_Retain(pipe);
        Pipe_Open(pipe, (mode & O_RDONLY) == O_RDONLY ? kPipeEnd_Read : kPipeEnd_Write);
    }

    *pOutSelf = (HandlerRef)self;
    return err;
}

errno_t PipeHandler_finalize(PipeHandlerRef _Nonnull self)
{
    PipeRef pp = _get_pipe();

    Pipe_Close(pp, (Handler_IsReadable(self)) ? kPipeEnd_Read : kPipeEnd_Write);
    Object_Release(pp);
    
    return EOK;
}

errno_t PipeHandler_read(PipeHandlerRef _Nonnull _Locked self, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
{
    PipeRef pp = _get_pipe();

    if (Handler_IsReadable(self)) {
        return Pipe_Read(pp, pBuffer, nBytesToRead, nOutBytesRead);
    }
    else {
        *nOutBytesRead = 0;
        return EBADF;
    }
}

errno_t PipeHandler_write(PipeHandlerRef _Nonnull _Locked self, const void* _Nonnull pBuffer, ssize_t nBytesToWrite, ssize_t* _Nonnull nOutBytesWritten)
{
    PipeRef pp = _get_pipe();

    if (Handler_IsWritable(self)) {
        return Pipe_Write(pp, pBuffer, nBytesToWrite, nOutBytesWritten);
    }
    else {
        *nOutBytesWritten = 0;
        return EBADF;
    }
}


class_func_defs(PipeHandler, Handler,
override_func_def(finalize, PipeHandler, Handler)
override_func_def(read, PipeHandler, Handler)
override_func_def(write, PipeHandler, Handler)
);
