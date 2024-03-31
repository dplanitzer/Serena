//
//  PipeChannel.c
//  kernel
//
//  Created by Dietmar Planitzer on 3/31/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "PipeChannel.h"
#include "Pipe.h"
#include <System/IOChannel.h>


errno_t PipeChannel_Create(ObjectRef _Nonnull pPipe, unsigned int mode, IOChannelRef _Nullable * _Nonnull pOutChannel)
{
    decl_try_err();
    PipeChannelRef self;

    try(IOChannel_AbstractCreate(&kPipeChannelClass, mode, (IOChannelRef*)&self));
    self->pipe = Object_Retain(pPipe);

catch:
    *pOutChannel = (IOChannelRef)self;
    return err;
}

void PipeChannel_deinit(PipeChannelRef _Nonnull self)
{
    Object_Release(self->pipe);
    self->pipe = NULL;
}

errno_t PipeChannel_dup(PipeChannelRef _Nonnull self, IOChannelRef _Nullable * _Nonnull pOutChannel)
{
    decl_try_err();
    PipeChannelRef pNewChannel;

    try(IOChannel_AbstractCreateCopy((IOChannelRef)self, (IOChannelRef*)&pNewChannel));
    pNewChannel->pipe = Object_Retain(self->pipe);

catch:
    *pOutChannel = (IOChannelRef)pNewChannel;
    return err;
}

errno_t PipeChannel_close(PipeChannelRef _Nonnull self)
{
    return Pipe_Close((PipeRef)self->pipe, IOChannel_GetMode(self));
}

errno_t PipeChannel_ioctl(PipeChannelRef _Nonnull self, int cmd, va_list ap)
{
    switch (cmd) {
        case kIOChannelCommand_GetType:
            *((int*) va_arg(ap, int*)) = kIOChannelType_File;
            return EOK;

        default:
            return Object_SuperN(ioctl, IOChannel, self, cmd, ap);
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


CLASS_METHODS(PipeChannel, IOChannel,
OVERRIDE_METHOD_IMPL(deinit, PipeChannel, Object)
OVERRIDE_METHOD_IMPL(close, PipeChannel, IOChannel)
OVERRIDE_METHOD_IMPL(dup, PipeChannel, IOChannel)
OVERRIDE_METHOD_IMPL(ioctl, PipeChannel, IOChannel)
OVERRIDE_METHOD_IMPL(read, PipeChannel, IOChannel)
OVERRIDE_METHOD_IMPL(write, PipeChannel, IOChannel)
);
