//
//  DriverChannel.c
//  kernel
//
//  Created by Dietmar Planitzer on 10/4/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "DriverChannel.h"
#include "Driver.h"
#include <kpi/fcntl.h>


errno_t DriverChannel_Create(Class* _Nonnull pClass, IOChannelOptions options, int channelType, unsigned int mode, DriverRef _Nonnull pDriver, IOChannelRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    DriverChannelRef self = NULL;

    if ((err = IOChannel_Create(pClass, options, SEO_FT_DRIVER, mode, (IOChannelRef*)&self)) == EOK) {
        mtx_init(&self->mtx);
        self->driver = Object_RetainAs(pDriver, Driver);
    }

    *pOutSelf = (IOChannelRef)self;
    return err;
}

errno_t DriverChannel_finalize(DriverChannelRef _Nonnull self)
{
    if (self->driver) {
        mtx_deinit(&self->mtx);
        Driver_Close(self->driver, (IOChannelRef)self);
        
        Object_Release(self->driver);
        self->driver = NULL;
    }

    return EOK;
}

void DriverChannel_lock(DriverChannelRef _Nonnull self)
{
  try_bang(mtx_lock(&self->mtx));
}

void DriverChannel_unlock(DriverChannelRef _Nonnull _Locked self)
{
  try_bang(mtx_unlock(&self->mtx));
}

errno_t DriverChannel_ioctl(DriverChannelRef _Nonnull _Locked self, int cmd, va_list ap)
{
    return Driver_vIoctl(self->driver, (IOChannelRef)self, cmd, ap);
}

errno_t DriverChannel_read(DriverChannelRef _Nonnull _Locked self, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
{
    return Driver_Read(self->driver, (IOChannelRef)self, pBuffer, nBytesToRead, nOutBytesRead);
}

errno_t DriverChannel_write(DriverChannelRef _Nonnull _Locked self, const void* _Nonnull pBuffer, ssize_t nBytesToWrite, ssize_t* _Nonnull nOutBytesWritten)
{
    return Driver_Write(self->driver, (IOChannelRef)self, pBuffer, nBytesToWrite, nOutBytesWritten);
}

off_t DriverChannel_getSeekableRange(DriverChannelRef _Nonnull _Locked self)
{
    return Driver_GetSeekableRange(self);
}


class_func_defs(DriverChannel, IOChannel,
override_func_def(finalize, DriverChannel, IOChannel)
override_func_def(lock, DriverChannel, IOChannel)
override_func_def(unlock, DriverChannel, IOChannel)
override_func_def(ioctl, DriverChannel, IOChannel)
override_func_def(read, DriverChannel, IOChannel)
override_func_def(write, DriverChannel, IOChannel)
override_func_def(getSeekableRange, DriverChannel, IOChannel)
);
