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
            return super_n(ioctl, IOChannel, self, cmd, ap);
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


class_func_defs(ConsoleChannel, IOChannel,
override_func_def(finalize, ConsoleChannel, IOChannel)
override_func_def(copy, ConsoleChannel, IOChannel)
override_func_def(ioctl, ConsoleChannel, IOChannel)
override_func_def(read, ConsoleChannel, IOChannel)
override_func_def(write, ConsoleChannel, IOChannel)
);
