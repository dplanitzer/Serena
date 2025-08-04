//
//  HandlerChannel.c
//  kernel
//
//  Created by Dietmar Planitzer on 8/3/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "HandlerChannel.h"
#include "Handler.h"


errno_t HandlerChannel_Create(HandlerRef _Nonnull hnd, IOChannelOptions options, int channelType, unsigned int mode, IOChannelRef _Nullable * _Nonnull pOutChannel)
{
    decl_try_err();
    HandlerChannelRef self;

    try(IOChannel_Create(&kHandlerChannelClass, options, channelType, mode, (IOChannelRef*)&self));
    self->hnd = Object_RetainAs(hnd, Handler);

catch:
    *pOutChannel = (IOChannelRef)self;
    return err;
}

errno_t HandlerChannel_finalize(HandlerChannelRef _Nonnull self)
{
    decl_try_err();

    Object_Release(self->hnd);
    self->hnd = NULL;

    return err;
}

void HandlerChannel_lock(HandlerChannelRef _Nonnull self)
{
}

void HandlerChannel_unlock(HandlerChannelRef _Nonnull _Locked self)
{
}

errno_t HandlerChannel_read(HandlerChannelRef _Nonnull _Locked self, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
{
    return Handler_Read(self->hnd, (IOChannelRef)self, pBuffer, nBytesToRead, nOutBytesRead);
}

errno_t HandlerChannel_write(HandlerChannelRef _Nonnull _Locked self, const void* _Nonnull pBuffer, ssize_t nBytesToWrite, ssize_t* _Nonnull nOutBytesWritten)
{
    return Handler_Write(self->hnd, (IOChannelRef)self, pBuffer, nBytesToWrite, nOutBytesWritten);
}

errno_t HandlerChannel_seek(HandlerChannelRef _Nonnull _Locked self, off_t offset, off_t* _Nullable pOutOldPosition, int whence)
{
    return Handler_Seek(self->hnd, (IOChannelRef)self, offset, pOutOldPosition, whence);
}

errno_t HandlerChannel_ioctl(HandlerChannelRef _Nonnull self, int cmd, va_list ap)
{
    return Handler_vIoctl(self->hnd, (IOChannelRef)self, cmd, ap);
}

class_func_defs(HandlerChannel, IOChannel,
override_func_def(finalize, HandlerChannel, IOChannel)
override_func_def(lock, HandlerChannel, IOChannel)
override_func_def(unlock, HandlerChannel, IOChannel)
override_func_def(read, HandlerChannel, IOChannel)
override_func_def(write, HandlerChannel, IOChannel)
override_func_def(seek, HandlerChannel, IOChannel)
override_func_def(ioctl, HandlerChannel, IOChannel)
);
