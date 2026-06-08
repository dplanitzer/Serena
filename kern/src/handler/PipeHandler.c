//
//  PipeHandler.c
//  kernel
//
//  Created by Dietmar Planitzer on 3/31/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "PipeHandler.h"
#include <ipc/Pipe.h>


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

    err = Handler_Create(&kPipeHandlerClass, FD_TYPE_INODE, mode, (HandlerRef*)&self);
    if (err == EOK) {
        self->pipe = Object_Retain(pipe);
        Pipe_Open(self->pipe, (mode & O_RDONLY) == O_RDONLY ? kPipeEnd_Read : kPipeEnd_Write);
    }

    *pOutSelf = (HandlerRef)self;
    return err;
}

void PipeHandler_deinit(PipeHandlerRef _Nonnull self)
{
    Object_Release(self->pipe);
    self->pipe = NULL;
}

errno_t PipeHandler_close(PipeHandlerRef _Nonnull self)
{
    Pipe_Close(self->pipe, (Handler_IsReadable(self)) ? kPipeEnd_Read : kPipeEnd_Write);
    
    return EOK;
}

errno_t PipeHandler_read(PipeHandlerRef _Nonnull _Locked self, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
{
    if (Handler_IsReadable(self)) {
        return Pipe_Read(self->pipe, pBuffer, nBytesToRead, nOutBytesRead);
    }
    else {
        *nOutBytesRead = 0;
        return EBADF;
    }
}

errno_t PipeHandler_write(PipeHandlerRef _Nonnull _Locked self, const void* _Nonnull pBuffer, ssize_t nBytesToWrite, ssize_t* _Nonnull nOutBytesWritten)
{
    if (Handler_IsWritable(self)) {
        return Pipe_Write(self->pipe, pBuffer, nBytesToWrite, nOutBytesWritten);
    }
    else {
        *nOutBytesWritten = 0;
        return EBADF;
    }
}


class_func_defs(PipeHandler, Handler,
override_func_def(deinit, PipeHandler, Object)
override_func_def(close, PipeHandler, Handler)
override_func_def(read, PipeHandler, Handler)
override_func_def(write, PipeHandler, Handler)
);
