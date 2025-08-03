//
//  Handler.c
//  kernel
//
//  Created by Dietmar Planitzer on 8/3/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "Handler.h"


errno_t Handler_open(HandlerRef _Nonnull _Locked self, unsigned int mode, intptr_t arg, IOChannelRef _Nullable * _Nonnull pOutChannel)
{
    return EPERM;
}

errno_t Handler_close(HandlerRef _Nonnull _Locked self, IOChannelRef _Nonnull ioc)
{
    return EOK;
}

errno_t Handler_read(HandlerRef _Nonnull self, IOChannelRef _Nonnull ioc, void* _Nonnull buf, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
{
    return EBADF;
}

errno_t Handler_write(HandlerRef _Nonnull self, IOChannelRef _Nonnull ioc, const void* _Nonnull buf, ssize_t nBytesToWrite, ssize_t* _Nonnull nOutBytesWritten)
{
    return EBADF;
}

errno_t Handler_seek(HandlerRef _Nonnull self, IOChannelRef _Nonnull ioc, off_t offset, off_t* _Nullable pOutOldPosition, int whence)
{
    return EPIPE;
}

errno_t Handler_ioctl(HandlerRef _Nonnull self, IOChannelRef _Nonnull ioc, int cmd, va_list ap)
{
    return ENOTIOCTLCMD;
}

errno_t Handler_Ioctl(HandlerRef _Nonnull self, IOChannelRef _Nonnull ioc, int cmd, ...)
{
    va_list ap;
    va_start(ap, cmd);
    const errno_t err = Handler_vIoctl(self, ioc, cmd, ap);
    va_end(ap);

    return err;
}


class_func_defs(Handler, Object,
func_def(open, Handler)
func_def(close, Handler)
func_def(read, Handler)
func_def(write, Handler)
func_def(seek, Handler)
func_def(ioctl, Handler)
);
