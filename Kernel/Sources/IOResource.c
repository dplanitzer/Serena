//
//  IOResource.c
//  kernel
//
//  Created by Dietmar Planitzer on 10/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "IOResource.h"
#include <System/IOChannel.h>

////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: IOChannel
////////////////////////////////////////////////////////////////////////////////

// Creates an instance of an I/O channel. Subclassers should call this method in
// their own constructor implementation and then initialize the subclass specific
// properties. 
errno_t IOChannel_AbstractCreate(ClassRef _Nonnull pClass, IOResourceRef _Nonnull pResource, unsigned int mode, IOChannelRef _Nullable * _Nonnull pOutChannel)
{
    decl_try_err();
    IOChannelRef pChannel;

    try(_Object_Create(pClass, 0, (ObjectRef*)&pChannel));
    pChannel->resource = Object_RetainAs(pResource, IOResource);
    pChannel->mode = mode & (kOpen_ReadWrite | kOpen_Append);

catch:
    *pOutChannel = pChannel;
    return err;
}

// Creates a copy of the given I/O channel. Subclassers should call this in their
// own copying implementation and then copy the subclass specific properties.
errno_t IOChannel_AbstractCreateCopy(IOChannelRef _Nonnull pInChannel, IOChannelRef _Nullable * _Nonnull pOutChannel)
{
    decl_try_err();
    IOChannelRef pChannel;

    try(_Object_Create(Object_GetClass(pInChannel), 0, (ObjectRef*)&pChannel));
    pChannel->resource = Object_RetainAs(pInChannel->resource, IOResource);
    pChannel->mode = pInChannel->mode;

catch:
    *pOutChannel = pChannel;
    return err;
}

ssize_t IOChannel_dup(IOChannelRef _Nonnull self, IOChannelRef _Nullable * _Nonnull pOutChannel)
{
    return IOResource_Dup(self->resource, self, pOutChannel);
}

errno_t IOChannel_ioctl(IOChannelRef _Nonnull self, int cmd, va_list ap)
{
    switch (cmd) {
        case kIOChannelCommand_GetMode:
            *((unsigned int*) va_arg(ap, unsigned int*)) = self->mode;
            return EOK;

        default:
            return ENOTIOCTLCMD;
    }}

errno_t IOChannel_vIOControl(IOChannelRef _Nonnull self, int cmd, va_list ap)
{
    if (IsIOChannelCommand(cmd)) {
        return Object_InvokeN(ioctl, IOChannel, self, cmd, ap);
    }
    else {
        return IOResource_vIOControl(self->resource, cmd, ap);
    }
}

errno_t IOChannel_read(IOChannelRef _Nonnull self, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
{
    if ((self->mode & kOpen_Read) == kOpen_Read) {
        return IOResource_Read(self->resource, self, pBuffer, nBytesToRead, nOutBytesRead);
    } else {
        *nOutBytesRead = 0;
        return EBADF;
    }
}

errno_t IOChannel_write(IOChannelRef _Nonnull self, const void* _Nonnull pBuffer, ssize_t nBytesToWrite, ssize_t* _Nonnull nOutBytesWritten)
{
    if ((self->mode & kOpen_Write) == kOpen_Write) {
        return IOResource_Write(self->resource, self, pBuffer, nBytesToWrite, nOutBytesWritten);
    } else {
        *nOutBytesWritten = 0;
        return EBADF;
    }
}

errno_t IOChannel_seek(IOChannelRef _Nonnull self, FileOffset offset, FileOffset* pOutPosition, int whence)
{
    *pOutPosition = 0;
    return ESPIPE;
}

errno_t IOChannel_close(IOChannelRef _Nonnull self)
{
    return IOResource_Close(self->resource, self);
}

void IOChannel_deinit(IOChannelRef _Nonnull self)
{
    Object_Release(self->resource);
    self->resource = NULL;
}

CLASS_METHODS(IOChannel, Object,
METHOD_IMPL(dup, IOChannel)
METHOD_IMPL(ioctl, IOChannel)
METHOD_IMPL(read, IOChannel)
METHOD_IMPL(write, IOChannel)
METHOD_IMPL(seek, IOChannel)
METHOD_IMPL(close, IOChannel)
OVERRIDE_METHOD_IMPL(deinit, IOChannel, Object)
);


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: IOResource
////////////////////////////////////////////////////////////////////////////////

// Opens a resource context/channel to the resource. This new resource context
// will be represented by a (file) descriptor in user space. The resource context
// maintains state that is specific to this connection. This state will be
// protected by the resource's internal locking mechanism. 'pNode' represents
// the named resource instance that should be represented by the I/O channel.
errno_t IOResource_open(IOResourceRef _Nonnull self, InodeRef _Nonnull _Locked pNode, unsigned int mode, User user, IOChannelRef _Nullable * _Nonnull pOutChannel)
{
    *pOutChannel = NULL;
    return EBADF;
}

// Creates an independent copy of the passed in I/O channel. Note that this function
// is allowed to return a strong reference to the channel that was passed in if
// the channel state is immutable. 
errno_t IOResource_dup(IOResourceRef _Nonnull self, IOChannelRef _Nonnull pChannel, IOChannelRef _Nullable * _Nonnull pOutChannel)
{
    *pOutChannel = NULL;
    return EBADF;
}

errno_t IOResource_read(IOResourceRef _Nonnull self, IOChannelRef _Nonnull pChannel, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
{
    *nOutBytesRead = 0;
    return EBADF;
}

errno_t IOResource_write(IOResourceRef _Nonnull self, IOChannelRef _Nonnull pChannel, const void* _Nonnull pBuffer, ssize_t nBytesToWrite, ssize_t* _Nonnull nOutBytesWritten)
{
    *nOutBytesWritten = 0;
    return EBADF;
}

// See IOChannel.close()
errno_t IOResource_close(IOResourceRef _Nonnull self, IOChannelRef _Nonnull pChannel)
{
    return EOK;
}

// Executes the resource specific command 'cmd'.
errno_t IOResource_ioctl(IOResourceRef _Nonnull self, int cmd, va_list ap)
{
    return ENOTIOCTLCMD;
}


CLASS_METHODS(IOResource, Object,
METHOD_IMPL(open, IOResource)
METHOD_IMPL(dup, IOResource)
METHOD_IMPL(ioctl, IOResource)
METHOD_IMPL(read, IOResource)
METHOD_IMPL(write, IOResource)
METHOD_IMPL(close, IOResource)
);
