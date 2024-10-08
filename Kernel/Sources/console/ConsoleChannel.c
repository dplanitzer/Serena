//
//  ConsoleChannel.c
//  kernel
//
//  Created by Dietmar Planitzer on 3/29/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "ConsoleChannel.h"
#include "ConsolePriv.h"


errno_t ConsoleChannel_Create(ConsoleRef _Nonnull pConsole, unsigned int mode, IOChannelRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    ConsoleChannelRef self;

    try(IOChannel_AbstractCreate(&kConsoleChannelClass, mode, (IOChannelRef*)&self));
    self->console = Object_RetainAs(pConsole, Console);

catch:
    *pOutSelf = (IOChannelRef)self;
    return err;
}

errno_t ConsoleChannel_finalize(ConsoleChannelRef _Nonnull self)
{
    Object_Release(self->console);
    self->console = NULL;

    return EOK;
}

errno_t ConsoleChannel_copy(ConsoleChannelRef _Nonnull self, IOChannelRef _Nullable * _Nonnull pOutChannel)
{
    decl_try_err();
    ConsoleChannelRef pNewChannel;

    try(IOChannel_AbstractCreate(classof(self), IOChannel_GetMode(self), (IOChannelRef*)&pNewChannel));
    pNewChannel->console = Object_RetainAs(self->console, Console);

catch:
    *pOutChannel = (IOChannelRef)pNewChannel;
    return err;
}

errno_t ConsoleChannel_ioctl(ConsoleChannelRef _Nonnull self, int cmd, va_list ap)
{
    switch (cmd) {
        case kIOChannelCommand_GetType:
            *((int*) va_arg(ap, int*)) = kIOChannelType_Terminal;
            return EOK;

        default:
            return super_n(ioctl, IOChannel, self, cmd, ap);
    }
}

errno_t ConsoleChannel_read(ConsoleChannelRef _Nonnull self, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
{
    return Driver_Read((DriverRef)self->console, (IOChannelRef)self, pBuffer, nBytesToRead, nOutBytesRead);
}

errno_t ConsoleChannel_write(ConsoleChannelRef _Nonnull self, const void* _Nonnull pBuffer, ssize_t nBytesToWrite, ssize_t* _Nonnull nOutBytesWritten)
{
    return Driver_Write((DriverRef)self->console, (IOChannelRef)self, pBuffer, nBytesToWrite, nOutBytesWritten);
}


class_func_defs(ConsoleChannel, IOChannel,
override_func_def(finalize, ConsoleChannel, IOChannel)
override_func_def(copy, ConsoleChannel, IOChannel)
override_func_def(ioctl, ConsoleChannel, IOChannel)
override_func_def(read, ConsoleChannel, IOChannel)
override_func_def(write, ConsoleChannel, IOChannel)
);
