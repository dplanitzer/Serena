//
//  DriverChannel.c
//  kernel
//
//  Created by Dietmar Planitzer on 8/3/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "DriverChannel.h"
#include "Driver.h"
#include <kern/kalloc.h>

#define _get_drv() \
IOChannel_GetResourceAs(self, Driver)


errno_t DriverChannel_Create(DriverRef _Nonnull drv, int channelType, unsigned int mode, size_t nExtraBytes, IOChannelRef _Nullable * _Nonnull pOutChannel)
{
    decl_try_err();
    DriverChannelRef self;

    try(IOChannel_Create(&kDriverChannelClass, channelType, mode, (intptr_t)drv, (IOChannelRef*)&self));
    if (nExtraBytes > 0) {
        try(kalloc_cleared(nExtraBytes, (void**)&self->extras));
        mtx_init(&self->ser_mtx);
    }
    Object_Retain(drv);

catch:
    *pOutChannel = (IOChannelRef)self;
    return err;
}

errno_t DriverChannel_finalize(DriverChannelRef _Nonnull self)
{
    decl_try_err();
    DriverRef drv = _get_drv();

    err = Driver_Close(drv, (IOChannelRef)self);
    Object_Release(drv);

    kfree(self->extras);
    self->extras = NULL;
    
    mtx_deinit(&self->ser_mtx);

    return err;
}

errno_t DriverChannel_read(DriverChannelRef _Nonnull _Locked self, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
{
    decl_try_err();

    if (IOChannel_IsReadable(self)) {
        mtx_lock(&self->ser_mtx);
        err = Driver_Read(_get_drv(), (IOChannelRef)self, pBuffer, nBytesToRead, nOutBytesRead);
        mtx_unlock(&self->ser_mtx);
    }
    else {
        *nOutBytesRead = 0;
        err = EBADF;
    }

    return err;
}

errno_t DriverChannel_write(DriverChannelRef _Nonnull _Locked self, const void* _Nonnull pBuffer, ssize_t nBytesToWrite, ssize_t* _Nonnull nOutBytesWritten)
{
    decl_try_err();

    if (IOChannel_IsWritable(self)) {
        mtx_lock(&self->ser_mtx);
        err = Driver_Write(_get_drv(), (IOChannelRef)self, pBuffer, nBytesToWrite, nOutBytesWritten);
        mtx_unlock(&self->ser_mtx);
    }
    else {
        *nOutBytesWritten = 0;
        err = EBADF;
    }

    return err;
}

errno_t DriverChannel_seek(DriverChannelRef _Nonnull _Locked self, off_t offset, off_t* _Nullable pOutNewPos, int whence)
{
    decl_try_err();

    if (Driver_IsSeekable(_get_drv())) {
        mtx_lock(&self->ser_mtx);
        err = IOChannel_DoSeek((IOChannelRef)self, offset, pOutNewPos, whence);
        mtx_unlock(&self->ser_mtx);
    }
    else {
        err = ESPIPE;
    }

    return err;
}

off_t DriverChannel_getSeekableRange(DriverChannelRef _Nonnull _Locked self)
{
    return Driver_GetSeekableRange(_get_drv());
}

errno_t DriverChannel_ioctl(DriverChannelRef _Nonnull self, int cmd, va_list ap)
{
    decl_try_err();

    mtx_lock(&self->ser_mtx);
    err = Driver_vIoctl(_get_drv(), (IOChannelRef)self, cmd, ap);
    mtx_unlock(&self->ser_mtx);

    return err;
}

class_func_defs(DriverChannel, IOChannel,
override_func_def(finalize, DriverChannel, IOChannel)
override_func_def(read, DriverChannel, IOChannel)
override_func_def(write, DriverChannel, IOChannel)
override_func_def(seek, DriverChannel, IOChannel)
override_func_def(getSeekableRange, DriverChannel, IOChannel)
override_func_def(ioctl, DriverChannel, IOChannel)
);
