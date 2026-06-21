//
//  DriverHandler.c
//  kernel
//
//  Created by Dietmar Planitzer on 8/3/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "DriverHandler.h"
#include <driver/Driver.h>


errno_t DriverHandler_Create(DriverRef _Nonnull drv, int channelType, unsigned int mode, HandlerRef _Nullable * _Nonnull pOutHandler)
{
    decl_try_err();
    DriverHandlerRef self;

    try(Handler_Create(&kDriverHandlerClass, channelType, mode, (HandlerRef*)&self));
    self->driver = Object_Retain(drv);
    mtx_init(&self->ser_mtx);

catch:
    *pOutHandler = (HandlerRef)self;
    return err;
}

void DriverHandler_deinit(DriverHandlerRef _Nonnull self)
{
    Driver_Close(self->driver);
    Object_Release(self->driver);
    self->driver = NULL;

    mtx_deinit(&self->ser_mtx);
}

errno_t DriverHandler_read(DriverHandlerRef _Nonnull _Locked self, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
{
    decl_try_err();
    const fd_flags_t flags = Handler_GetFlags(self);

    if ((flags & O_RDONLY) == 0) {
        *nOutBytesRead = 0;
        return EBADF;
    }


    mtx_lock(&self->ser_mtx);
    err = Driver_Read(self->driver, flags, &self->offset, pBuffer, nBytesToRead, nOutBytesRead);
    mtx_unlock(&self->ser_mtx);

    return err;
}

errno_t DriverHandler_write(DriverHandlerRef _Nonnull _Locked self, const void* _Nonnull pBuffer, ssize_t nBytesToWrite, ssize_t* _Nonnull nOutBytesWritten)
{
    decl_try_err();
    const fd_flags_t flags = Handler_GetFlags(self);

    if ((flags & O_WRONLY) == 0) {
        *nOutBytesWritten = 0;
        return EBADF;
    }


    mtx_lock(&self->ser_mtx);
    err = Driver_Write(self->driver, flags, &self->offset, pBuffer, nBytesToWrite, nOutBytesWritten);
    mtx_unlock(&self->ser_mtx);

    return err;
}

errno_t DriverHandler_seek(DriverHandlerRef _Nonnull _Locked self, off_t offset, off_t* _Nullable pOutNewPos, int whence)
{
    decl_try_err();
    off_t endPos = 0ll;

    if (!Driver_IsSeekable(self->driver)) {
        return ESPIPE;
    }


    mtx_lock(&self->ser_mtx);
    if (whence == SEEK_END) {
        endPos = Driver_GetSeekableRange(self->driver);
    }

    err = do_seek(offset, whence, endPos, &self->offset);

    if (pOutNewPos && err == EOK) {
        *pOutNewPos = self->offset;
    }
    mtx_unlock(&self->ser_mtx);

    return err;
}

errno_t DriverHandler_control(DriverHandlerRef _Nonnull self, int cmd, va_list ap)
{
    decl_try_err();
    const fd_flags_t flags = Handler_GetFlags(self);

    mtx_lock(&self->ser_mtx);
    err = Driver_vIoctl(self->driver, flags, &self->offset, cmd, ap);
    mtx_unlock(&self->ser_mtx);

    return err;
}


class_func_defs(DriverHandler, Handler,
override_func_def(deinit, DriverHandler, Object)
override_func_def(read, DriverHandler, Handler)
override_func_def(write, DriverHandler, Handler)
override_func_def(seek, DriverHandler, Handler)
override_func_def(control, DriverHandler, Handler)
);
