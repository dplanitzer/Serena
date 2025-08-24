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


errno_t DriverChannel_Create(DriverRef _Nonnull drv, int channelType, unsigned int mode, size_t nExtraBytes, IOChannelRef _Nullable * _Nonnull pOutChannel)
{
    decl_try_err();
    DriverChannelRef self;

    try(IOChannel_Create(&kDriverChannelClass, 0, channelType, mode, (IOChannelRef*)&self));
    if (nExtraBytes > 0) {
        try(kalloc_cleared(nExtraBytes, (void**)&self->extras));
    }
    self->drv = Object_RetainAs(drv, Driver);

catch:
    *pOutChannel = (IOChannelRef)self;
    return err;
}

errno_t DriverChannel_finalize(DriverChannelRef _Nonnull self)
{
    decl_try_err();

    err = Driver_Close(self->drv, (IOChannelRef)self);
    Object_Release(self->drv);
    self->drv = NULL;

    kfree(self->extras);
    self->extras = NULL;
    
    return err;
}

errno_t DriverChannel_read(DriverChannelRef _Nonnull _Locked self, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
{
    if (IOChannel_IsReadable(self)) {
        return Driver_Read(self->drv, (IOChannelRef)self, pBuffer, nBytesToRead, nOutBytesRead);
    }
    else {
        *nOutBytesRead = 0;
        return EBADF;
    }
}

errno_t DriverChannel_write(DriverChannelRef _Nonnull _Locked self, const void* _Nonnull pBuffer, ssize_t nBytesToWrite, ssize_t* _Nonnull nOutBytesWritten)
{
    if (IOChannel_IsWritable(self)) {
        return Driver_Write(self->drv, (IOChannelRef)self, pBuffer, nBytesToWrite, nOutBytesWritten);
    }
    else {
        *nOutBytesWritten = 0;
        return EBADF;
    }
}

errno_t DriverChannel_seek(DriverChannelRef _Nonnull _Locked self, off_t offset, off_t* _Nullable pOutNewPos, int whence)
{
    if (Driver_IsSeekable(self->drv)) {
        return IOChannel_DoSeek((IOChannelRef)self, offset, pOutNewPos, whence);
    }
    else {
        return ESPIPE;
    }
}

off_t DriverChannel_getSeekableRange(DriverChannelRef _Nonnull _Locked self)
{
    return Driver_GetSeekableRange(self->drv);
}

errno_t DriverChannel_ioctl(DriverChannelRef _Nonnull self, int cmd, va_list ap)
{
    return Driver_vIoctl(self->drv, (IOChannelRef)self, cmd, ap);
}

class_func_defs(DriverChannel, IOChannel,
override_func_def(finalize, DriverChannel, IOChannel)
override_func_def(read, DriverChannel, IOChannel)
override_func_def(write, DriverChannel, IOChannel)
override_func_def(seek, DriverChannel, IOChannel)
override_func_def(getSeekableRange, DriverChannel, IOChannel)
override_func_def(ioctl, DriverChannel, IOChannel)
);
