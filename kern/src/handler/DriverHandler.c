//
//  DriverHandler.c
//  kernel
//
//  Created by Dietmar Planitzer on 8/3/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "DriverHandler.h"
#include <driver/Driver.h>
#include <kern/kalloc.h>

#define _get_drv() \
Handler_GetResourceAs(self, Driver)


errno_t DriverHandler_Create(DriverRef _Nonnull drv, int channelType, unsigned int mode, size_t nExtraBytes, HandlerRef _Nullable * _Nonnull pOutHandler)
{
    decl_try_err();
    DriverHandlerRef self;

    try(Handler_Create(&kDriverHandlerClass, channelType, mode, (intptr_t)drv, (HandlerRef*)&self));
    if (nExtraBytes > 0) {
        try(kalloc_cleared(nExtraBytes, (void**)&self->extras));
        mtx_init(&self->ser_mtx);
    }
    Object_Retain(drv);

catch:
    *pOutHandler = (HandlerRef)self;
    return err;
}

void DriverHandler_deinit(DriverHandlerRef _Nonnull self)
{
    Object_Release(_get_drv());

    kfree(self->extras);
    self->extras = NULL;
    
    mtx_deinit(&self->ser_mtx);
}

errno_t DriverHandler_shutdown(DriverHandlerRef _Nonnull self)
{
    return Driver_Close(_get_drv(), (HandlerRef)self);
}

errno_t DriverHandler_read(DriverHandlerRef _Nonnull _Locked self, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
{
    decl_try_err();

    if (Handler_IsReadable(self)) {
        mtx_lock(&self->ser_mtx);
        err = Driver_Read(_get_drv(), (HandlerRef)self, pBuffer, nBytesToRead, nOutBytesRead);
        mtx_unlock(&self->ser_mtx);
    }
    else {
        *nOutBytesRead = 0;
        err = EBADF;
    }

    return err;
}

errno_t DriverHandler_write(DriverHandlerRef _Nonnull _Locked self, const void* _Nonnull pBuffer, ssize_t nBytesToWrite, ssize_t* _Nonnull nOutBytesWritten)
{
    decl_try_err();

    if (Handler_IsWritable(self)) {
        mtx_lock(&self->ser_mtx);
        err = Driver_Write(_get_drv(), (HandlerRef)self, pBuffer, nBytesToWrite, nOutBytesWritten);
        mtx_unlock(&self->ser_mtx);
    }
    else {
        *nOutBytesWritten = 0;
        err = EBADF;
    }

    return err;
}

errno_t DriverHandler_seek(DriverHandlerRef _Nonnull _Locked self, off_t offset, off_t* _Nullable pOutNewPos, int whence)
{
    decl_try_err();

    if (Driver_IsSeekable(_get_drv())) {
        mtx_lock(&self->ser_mtx);
        err = Handler_DoSeek((HandlerRef)self, offset, pOutNewPos, whence);
        mtx_unlock(&self->ser_mtx);
    }
    else {
        err = ESPIPE;
    }

    return err;
}

off_t DriverHandler_getSeekableRange(DriverHandlerRef _Nonnull _Locked self)
{
    return Driver_GetSeekableRange(_get_drv());
}

errno_t DriverHandler_ioctl(DriverHandlerRef _Nonnull self, int cmd, va_list ap)
{
    decl_try_err();

    mtx_lock(&self->ser_mtx);
    err = Driver_vIoctl(_get_drv(), (HandlerRef)self, cmd, ap);
    mtx_unlock(&self->ser_mtx);

    return err;
}

class_func_defs(DriverHandler, Handler,
override_func_def(deinit, DriverHandler, Object)
override_func_def(shutdown, DriverHandler, Handler)
override_func_def(read, DriverHandler, Handler)
override_func_def(write, DriverHandler, Handler)
override_func_def(seek, DriverHandler, Handler)
override_func_def(getSeekableRange, DriverHandler, Handler)
override_func_def(ioctl, DriverHandler, Handler)
);
