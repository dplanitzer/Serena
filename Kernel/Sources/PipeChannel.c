//
//  PipeChannel.c
//  kernel
//
//  Created by Dietmar Planitzer on 3/31/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "PipeChannel.h"
#include "Pipe.h"


errno_t PipeChannel_Create(ObjectRef _Nonnull pPipe, unsigned int mode, IOChannelRef _Nullable * _Nonnull pOutChannel)
{
    decl_try_err();
    PipeChannelRef self;

    assert((mode & kOpen_ReadWrite) == kOpen_Read || (mode & kOpen_ReadWrite) == kOpen_Write);

    try(IOChannel_AbstractCreate(&kPipeChannelClass, mode, (IOChannelRef*)&self));
    self->pipe = Object_Retain(pPipe);

catch:
    *pOutChannel = (IOChannelRef)self;
    return err;
}

errno_t PipeChannel_finalize(PipeChannelRef _Nonnull self)
{
    Pipe_Close((PipeRef)self->pipe, IOChannel_GetMode(self));

    Object_Release(self->pipe);
    self->pipe = NULL;
    
    return EOK;
}

errno_t PipeChannel_copy(PipeChannelRef _Nonnull self, IOChannelRef _Nullable * _Nonnull pOutChannel)
{
    decl_try_err();
    PipeChannelRef pNewChannel;

    try(IOChannel_AbstractCreate(classof(self), IOChannel_GetMode(self), (IOChannelRef*)&pNewChannel));
    pNewChannel->pipe = Object_Retain(self->pipe);

catch:
    *pOutChannel = (IOChannelRef)pNewChannel;
    return err;
}

errno_t PipeChannel_ioctl(PipeChannelRef _Nonnull self, int cmd, va_list ap)
{
    switch (cmd) {
        case kIOChannelCommand_GetType:
            *((int*) va_arg(ap, int*)) = kIOChannelType_Pipe;
            return EOK;

        default:
            return super_n(ioctl, IOChannel, self, cmd, ap);
    }
}

errno_t PipeChannel_read(PipeChannelRef _Nonnull self, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
{
    return Pipe_Read((PipeRef)self->pipe, pBuffer, nBytesToRead, nOutBytesRead);
}

errno_t PipeChannel_write(PipeChannelRef _Nonnull self, const void* _Nonnull pBuffer, ssize_t nBytesToWrite, ssize_t* _Nonnull nOutBytesWritten)
{
    return Pipe_Write((PipeRef)self->pipe, pBuffer, nBytesToWrite, nOutBytesWritten);
}


class_func_defs(PipeChannel, IOChannel,
override_func_def(finalize, PipeChannel, IOChannel)
override_func_def(copy, PipeChannel, IOChannel)
override_func_def(ioctl, PipeChannel, IOChannel)
override_func_def(read, PipeChannel, IOChannel)
override_func_def(write, PipeChannel, IOChannel)
);
