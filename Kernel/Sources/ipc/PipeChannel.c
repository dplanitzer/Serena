//
//  PipeChannel.c
//  kernel
//
//  Created by Dietmar Planitzer on 3/31/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "PipeChannel.h"
#include "Pipe.h"
#include <kpi/fcntl.h>


// PipeChannel does not need locking because:
// - it doesn't support seeking. Thus seeking state is constant
// - the pipe implementation protects read/write with a pipe lock anyway
 
errno_t PipeChannel_Create(PipeRef _Nonnull pipe, unsigned int mode, IOChannelRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    PipeChannelRef self;

    if ((mode & O_RDWR) == 0 || (mode & O_RDWR) == O_RDWR) {
        return EACCESS;
    }

    err = IOChannel_Create(&kPipeChannelClass, 0, kIOChannelType_Pipe, mode, (IOChannelRef*)&self);
    if (err == EOK) {
        self->pipe = Object_RetainAs(pipe, Pipe);
        Pipe_Open(pipe, (mode & O_RDONLY) == O_RDONLY ? kPipeEnd_Read : kPipeEnd_Write);
    }

    *pOutSelf = (IOChannelRef)self;
    return err;
}

errno_t PipeChannel_finalize(PipeChannelRef _Nonnull self)
{
    Pipe_Close(self->pipe, (IOChannel_GetMode(self) & O_RDONLY) == O_RDONLY ? kPipeEnd_Read : kPipeEnd_Write);

    Object_Release(self->pipe);
    self->pipe = NULL;
    
    return EOK;
}

errno_t PipeChannel_read(PipeChannelRef _Nonnull _Locked self, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
{
    return Pipe_Read(self->pipe, pBuffer, nBytesToRead, nOutBytesRead);
}

errno_t PipeChannel_write(PipeChannelRef _Nonnull _Locked self, const void* _Nonnull pBuffer, ssize_t nBytesToWrite, ssize_t* _Nonnull nOutBytesWritten)
{
    return Pipe_Write(self->pipe, pBuffer, nBytesToWrite, nOutBytesWritten);
}


class_func_defs(PipeChannel, IOChannel,
override_func_def(finalize, PipeChannel, IOChannel)
override_func_def(read, PipeChannel, IOChannel)
override_func_def(write, PipeChannel, IOChannel)
);
