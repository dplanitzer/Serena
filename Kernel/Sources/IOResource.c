//
//  IOResource.c
//  Apollo
//
//  Created by Dietmar Planitzer on 10/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "IOResource.h"

////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: IOChannel
////////////////////////////////////////////////////////////////////////////////

ErrorCode IOChannel_Create(IOResourceRef _Nonnull pResource, UInt options, ByteCount stateSize, IOChannelRef _Nullable * _Nonnull pOutChannel)
{
    decl_try_err();
    IOChannelRef pChannel;

    try(Object_CreateWithExtraBytes(IOChannel, stateSize, &pChannel));
    pChannel->resource = Object_RetainAs(pResource, IOResource);
    pChannel->options = options;
    *pOutChannel = pChannel;
    return EOK;

catch:
    *pOutChannel = NULL;
    return err;
}

ErrorCode IOChannel_CreateCopy(IOChannelRef _Nonnull pInChannel, ByteCount stateSize, IOChannelRef _Nullable * _Nonnull pOutChannel)
{
    decl_try_err();
    IOChannelRef pChannel;

    try(Object_CreateWithExtraBytes(IOChannel, stateSize, &pChannel));
    pChannel->resource = Object_RetainAs(pInChannel->resource, IOResource);
    pChannel->options = pInChannel->options;
    Bytes_CopyRange(&pChannel->state[0], &pInChannel->state[0], stateSize);

    *pOutChannel = pChannel;
    return EOK;

catch:
    *pOutChannel = NULL;
    return err;
}

ByteCount IOChannel_dup(IOChannelRef _Nonnull self, IOChannelRef _Nullable * _Nonnull pOutChannel)
{
    return IOResource_Dup(self->resource, self, pOutChannel);
}

ByteCount IOChannel_command(IOChannelRef _Nonnull self, Int op, va_list ap)
{
    return IOResource_Command(self->resource, self->state, op, ap);
}

ByteCount IOChannel_read(IOChannelRef _Nonnull self, Byte* _Nonnull pBuffer, ByteCount nBytesToRead)
{
    if ((self->options & FREAD) == 0) {
        return -EBADF;
    }

    return IOResource_Read(self->resource, self->state, pBuffer, nBytesToRead);
}

ByteCount IOChannel_write(IOChannelRef _Nonnull self, const Byte* _Nonnull pBuffer, ByteCount nBytesToWrite)
{
    if ((self->options & FWRITE) == 0) {
        return -EBADF;
    }

    return IOResource_Write(self->resource, self->state, pBuffer, nBytesToWrite);
}

ErrorCode IOChannel_close(IOChannelRef _Nonnull self)
{
    return IOResource_Close(self->resource, self->state);
}

void IOChannel_deinit(IOChannelRef _Nonnull self)
{
    Object_Release(self->resource);
    self->resource = NULL;
}

CLASS_METHODS(IOChannel, Object,
METHOD_IMPL(dup, IOChannel)
METHOD_IMPL(command, IOChannel)
METHOD_IMPL(read, IOChannel)
METHOD_IMPL(write, IOChannel)
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
// protected by the resource's internal locking mechanism.
ErrorCode IOResource_open(IOResourceRef _Nonnull self, const Character* _Nonnull pPath, UInt options, IOChannelRef _Nullable * _Nonnull pOutChannel)
{
    *pOutChannel = NULL;
    return EBADF;
}

// Creates an independent copy of the passed in rescon. Note that this function
// is allowed to return a strong reference to the rescon that was passed in if
// the rescon state is immutable. 
ErrorCode IOResource_dup(IOResourceRef _Nonnull self, IOChannelRef _Nonnull pChannel, IOChannelRef _Nullable * _Nonnull pOutChannel)
{
    *pOutChannel = NULL;
    return EBADF;
}

// Executes the resource specific command 'op'.
ErrorCode IOResource_command(IOResourceRef _Nonnull self, void* _Nonnull pContext, Int op, va_list ap)
{
    return EBADF;
}

ByteCount IOResource_read(IOResourceRef _Nonnull self, void* _Nonnull pContext, Byte* _Nonnull pBuffer, ByteCount nBytesToRead)
{
    return -EBADF;
}

ByteCount IOResource_write(IOResourceRef _Nonnull self, void* _Nonnull pContext, const Byte* _Nonnull pBuffer, ByteCount nBytesToWrite)
{
    return -EBADF;
}

// See UObject.close()
ErrorCode IOResource_close(IOResourceRef _Nonnull self, void* _Nonnull pContext)
{
    return EOK;
}


CLASS_METHODS(IOResource, Object,
METHOD_IMPL(open, IOResource)
METHOD_IMPL(dup, IOResource)
METHOD_IMPL(command, IOResource)
METHOD_IMPL(read, IOResource)
METHOD_IMPL(write, IOResource)
METHOD_IMPL(close, IOResource)
);
