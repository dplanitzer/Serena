//
//  ConsoleChannel.c
//  kernel
//
//  Created by Dietmar Planitzer on 3/29/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "ConsoleChannel.h"
#include "ConsolePriv.h"


extern errno_t Console_Read(ConsoleRef _Nonnull self, ConsoleChannelRef _Nonnull pChannel, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead);
extern errno_t Console_Write(ConsoleRef _Nonnull self, const void* _Nonnull pBytes, ssize_t nBytesToWrite);


errno_t ConsoleChannel_Create(ObjectRef _Nonnull pConsole, unsigned int mode, IOChannelRef _Nullable * _Nonnull pOutChannel)
{
    decl_try_err();
    ConsoleChannelRef self;

    try(IOChannel_AbstractCreate(&kConsoleChannelClass, mode, (IOChannelRef*)&self));
    self->console = Object_Retain(pConsole);

catch:
    *pOutChannel = (IOChannelRef)self;
    return err;
}

void ConsoleChannel_deinit(ConsoleChannelRef _Nonnull self)
{
    Object_Release(self->console);
    self->console = NULL;
}

errno_t ConsoleChannel_dup(ConsoleChannelRef _Nonnull self, IOChannelRef _Nullable * _Nonnull pOutChannel)
{
    decl_try_err();
    ConsoleChannelRef pNewChannel;

    try(IOChannel_AbstractCreateCopy((IOChannelRef)self, (IOChannelRef*)&pNewChannel));
    pNewChannel->console = Object_Retain(self->console);

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
            return Object_SuperN(ioctl, IOChannel, self, cmd, ap);
    }
}

errno_t ConsoleChannel_read(ConsoleChannelRef _Nonnull self, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
{
    return Console_Read((ConsoleRef)self->console, self, pBuffer, nBytesToRead, nOutBytesRead);
}

errno_t ConsoleChannel_write(ConsoleChannelRef _Nonnull self, const void* _Nonnull pBuffer, ssize_t nBytesToWrite, ssize_t* _Nonnull nOutBytesWritten)
{
    const errno_t err = Console_Write((ConsoleRef)self->console, pBuffer, nBytesToWrite);
    *nOutBytesWritten = (err == EOK) ? nBytesToWrite : 0;
    return err;
}


CLASS_METHODS(ConsoleChannel, IOChannel,
OVERRIDE_METHOD_IMPL(deinit, ConsoleChannel, Object)
OVERRIDE_METHOD_IMPL(dup, ConsoleChannel, IOChannel)
OVERRIDE_METHOD_IMPL(ioctl, ConsoleChannel, IOChannel)
OVERRIDE_METHOD_IMPL(read, ConsoleChannel, IOChannel)
OVERRIDE_METHOD_IMPL(write, ConsoleChannel, IOChannel)
);
