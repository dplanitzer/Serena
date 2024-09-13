//
//  EventChannel.c
//  kernel
//
//  Created by Dietmar Planitzer on 3/31/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "EventChannel.h"
#include "EventDriver.h"


errno_t EventChannel_Create(ObjectRef _Nonnull pEventDriver, unsigned int mode, IOChannelRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    EventChannelRef self;

    try(IOChannel_AbstractCreate(&kEventChannelClass, mode, (IOChannelRef*)&self));
    self->eventDriver = Object_Retain(pEventDriver);
    self->timeout = kTimeInterval_Infinity;

catch:
    *pOutSelf = (IOChannelRef)self;
    return err;
}

errno_t EventChannel_finalize(EventChannelRef _Nonnull self)
{
    Object_Release(self->eventDriver);
    self->eventDriver = NULL;

    return EOK;
}

errno_t EventChannel_copy(EventChannelRef _Nonnull self, IOChannelRef _Nullable * _Nonnull pOutChannel)
{
    decl_try_err();
    EventChannelRef pNewChannel;

    try(IOChannel_AbstractCreate(classof(self), IOChannel_GetMode(self), (IOChannelRef*)&pNewChannel));
    pNewChannel->eventDriver = Object_Retain(self->eventDriver);
    pNewChannel->timeout = self->timeout;

catch:
    *pOutChannel = (IOChannelRef)pNewChannel;
    return err;
}

errno_t EventChannel_ioctl(EventChannelRef _Nonnull self, int cmd, va_list ap)
{
    switch (cmd) {
        case kIOChannelCommand_GetType:
            *((int*) va_arg(ap, int*)) = kIOChannelType_Driver;
            return EOK;

        default:
            return super_n(ioctl, IOChannel, self, cmd, ap);
    }
}

errno_t EventChannel_read(EventChannelRef _Nonnull self, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
{
    return EventDriver_Read((EventDriverRef)self->eventDriver, pBuffer, nBytesToRead, self->timeout, nOutBytesRead);
}


class_func_defs(EventChannel, IOChannel,
override_func_def(finalize, EventChannel, IOChannel)
override_func_def(copy, EventChannel, IOChannel)
override_func_def(ioctl, EventChannel, IOChannel)
override_func_def(read, EventChannel, IOChannel)
);
