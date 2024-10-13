//
//  DriverChannel.c
//  kernel
//
//  Created by Dietmar Planitzer on 10/4/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "DriverChannel.h"
#include "Driver.h"


errno_t DriverChannel_Create(Class* _Nonnull pClass, int channelType, unsigned int mode, DriverRef _Nonnull pDriver, IOChannelRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    DriverChannelRef self = NULL;

    if ((err = IOChannel_Create(pClass, kIOChannelType_Driver, mode, (IOChannelRef*)&self)) == EOK) {
        self->driver = Object_RetainAs(pDriver, Driver);
    }

    *pOutSelf = (IOChannelRef)self;
    return err;
}

errno_t DriverChannel_finalize(DriverChannelRef _Nonnull self)
{
    Object_Release(self->driver);
    self->driver = NULL;

    return EOK;
}

errno_t DriverChannel_copy(DriverChannelRef _Nonnull self, IOChannelRef _Nullable * _Nonnull pOutChannel)
{
    decl_try_err();
    DriverChannelRef pNewChannel = NULL;

    if ((err = IOChannel_Create(classof(self), IOChannel_GetChannelType(self), IOChannel_GetMode(self), (IOChannelRef*)&pNewChannel)) == EOK) {
        pNewChannel->driver = Object_RetainAs(self->driver, Driver);
    }

    *pOutChannel = (IOChannelRef)pNewChannel;
    return err;
}

errno_t DriverChannel_ioctl(DriverChannelRef _Nonnull self, int cmd, va_list ap)
{
    if (IsIOChannelCommand(cmd)) {
        return super_n(ioctl, IOChannel, DriverChannel, self, cmd, ap);
    }
    else {
        return Driver_Ioctl(self->driver, cmd, ap);
    }
}

errno_t DriverChannel_read(DriverChannelRef _Nonnull self, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
{
    return Driver_Read(self->driver, (IOChannelRef)self, pBuffer, nBytesToRead, nOutBytesRead);
}

errno_t DriverChannel_write(DriverChannelRef _Nonnull self, const void* _Nonnull pBuffer, ssize_t nBytesToWrite, ssize_t* _Nonnull nOutBytesWritten)
{
    return Driver_Write(self->driver, (IOChannelRef)self, pBuffer, nBytesToWrite, nOutBytesWritten);
}


class_func_defs(DriverChannel, IOChannel,
override_func_def(finalize, DriverChannel, IOChannel)
override_func_def(copy, DriverChannel, IOChannel)
override_func_def(ioctl, DriverChannel, IOChannel)
override_func_def(read, DriverChannel, IOChannel)
override_func_def(write, DriverChannel, IOChannel)
);
