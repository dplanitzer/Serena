//
//  IOResource.h
//  Apollo
//
//  Created by Dietmar Planitzer on 10/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef IOResource_h
#define IOResource_h

#include <klib/klib.h>

CLASS_FORWARD(IOResource);


OPEN_CLASS_WITH_REF(IOChannel, Object,
    IOResourceRef _Nonnull  resource;
    UInt                    options;
    Byte                    state[1];
);
enum IOChannelMethodIndex {
    kIOChannelMethodIndex_dup = kObjectMethodIndex_deinit + 1,
    kIOChannelMethodIndex_command,
    kIOChannelMethodIndex_read,
    kIOChannelMethodIndex_write,
    kIOChannelMethodIndex_seek,
    kIOChannelMethodIndex_close,

    kIOChannelMethodIndex_Count = kIOChannelMethodIndex_close + 1
};
METHOD_TYPE_N(ErrorCode, IOChannel, dup, IOChannelRef _Nullable * _Nonnull pOutChannel);
METHOD_TYPE_N(ErrorCode, IOChannel, command, Int op, va_list ap);
METHOD_TYPE_N(ByteCount, IOChannel, read, Byte* _Nonnull pBuffer, ByteCount nBytesToRead);
METHOD_TYPE_N(ByteCount, IOChannel, write, const Byte* _Nonnull pBuffer, ByteCount nBytesToWrite);
METHOD_TYPE_N(ErrorCode, IOChannel, seek, Int64 offset, Int64* _Nullable pOutPosition, Int whence);
METHOD_TYPE_0(ErrorCode, IOChannel, close);

#define FREAD   0x0001
#define FWRITE  0x0002


extern ErrorCode IOChannel_Create(IOResourceRef _Nonnull pResource, UInt options, ByteCount stateSize, IOChannelRef _Nullable * _Nonnull pOutChannel);

// Creates a copy of the given rescon with a state size of 'stateSize'.
extern ErrorCode IOChannel_CreateCopy(IOChannelRef _Nonnull pInChannel, ByteCount stateSize, IOChannelRef _Nullable * _Nonnull pOutChannel);

#define IOChannel_GetStateAs(__self, __type) \
    ((__type*) &__self->state[0])

#define IOChannel_Dup(__self, __pOutChannel) \
Object_InvokeN(dup, IOChannel, __self, __pOutChannel)

#define IOChannel_Command(__self, __op, __ap) \
Object_InvokeN(command, IOChannel, __self, __op, __ap)

#define IOChannel_Read(__self, __pBuffer, __nBytesToRead) \
Object_InvokeN(read, IOChannel, __self, __pBuffer, __nBytesToRead)

#define IOChannel_Write(__self, __pBuffer, __nBytesToWrite) \
Object_InvokeN(write, IOChannel, __self, __pBuffer, __nBytesToWrite)

#define IOChannel_Seek(__self, __offset, __pOutPosition, __whence) \
Object_InvokeN(seek, IOChannel, __self, __offset, __pOutPosition, __whence)

#define IOChannel_Close(__self) \
Object_Invoke0(close, IOChannel, __self)


////////////////////////////////////////////////////////////////////////////////

OPEN_CLASS(IOResource, Object,);
enum IOResourceMethodIndex {
    kIOResourceMethodIndex_open = kObjectMethodIndex_deinit + 1,
    kIOResourceMethodIndex_dup,
    kIOResourceMethodIndex_command,
    kIOResourceMethodIndex_read,
    kIOResourceMethodIndex_write,
    kIOResourceMethodIndex_seek,
    kIOResourceMethodIndex_close,

    kIOResourceMethodIndex_Count = kIOResourceMethodIndex_close + 1
};


// Opens a resource context/channel to the resource. This new resource context
// will be represented by a (file) descriptor in user space. The resource context
// maintains state that is specific to this connection. This state will be
// protected by the resource's internal locking mechanism.
METHOD_TYPE_N(ErrorCode, IOResource, open, const Character* _Nonnull pPath, UInt options, IOChannelRef _Nullable * _Nonnull pOutChannel);

// Creates an independent copy of the passed in I/O channel. Note that this function
// is allowed to return a strong reference to the channel that was passed in if
// the channel state is immutable. 
METHOD_TYPE_N(ErrorCode, IOResource, dup, IOChannelRef _Nonnull pChannel, IOChannelRef _Nullable * _Nonnull pOutChannel);

// Executes the resource specific command 'op'.
METHOD_TYPE_N(ErrorCode, IOResource, command, void* _Nonnull pChannel, Int op, va_list ap);

METHOD_TYPE_N(ByteCount, IOResource, read, void* _Nonnull pChannel, Byte* _Nonnull pBuffer, ByteCount nBytesToRead);
METHOD_TYPE_N(ByteCount, IOResource, write, void* _Nonnull pChannel, const Byte* _Nonnull pBuffer, ByteCount nBytesToWrite);

// Changes the position of the given I/O channel to the new location implied by
// the combination of 'offset' and 'whence'. Returns the new position and error
// status.
#define SEEK_SET    0
#define SEEK_CUR    1
#define SEEK_END    2
METHOD_TYPE_N(ErrorCode, IOResource, seek, void* _Nonnull pChannel, Int64 offset, Int64* _Nullable pOutPosition, Int whence);

// Close the resource. The purpose of the close operation is:
// - flush all data that was written and is still buffered/cached to the underlying device
// - if a write operation is ongoing at the time of the close then let this write operation finish and sync the underlying device
// - if a read operation is ongoing at the time of the close then interrupt the read with an EINTR error
// The resource should be internally marked as closed and all future read/write/etc operations on the resource should do nothing
// and instead return a suitable status. Eg a write should return EIO and a read should return EOF.
// It is permissible for a close operation to block the caller for some (reasonable) amount of time to complete the flush.
// The close operation may return an error. Returning an error will not stop the kernel from completing the close and eventually
// deallocating the resource. The error is passed on to the caller but is purely advisory in nature. The close operation is
// required to mark the resource as closed whether the close internally succeeded or failed. 
METHOD_TYPE_N(ErrorCode, IOResource, close, void* _Nonnull pChannel);


#define IOResource_Open(__self, __pPath, __options, __pOutChannel) \
Object_InvokeN(open, IOResource, __self, __pPath, __options, __pOutChannel)

#define IOResource_Dup(__self, __pChannel, __pOutChannel) \
Object_InvokeN(dup, IOResource, __self, __pChannel, __pOutChannel)

#define IOResource_Command(__self, __pChannel, __op, __ap) \
Object_InvokeN(command, IOResource, __self, __pChannel, __op, __ap)

#define IOResource_Read(__self, __pChannel, __pBuffer, __nBytesToRead) \
Object_InvokeN(read, IOResource, __self, __pChannel, __pBuffer, __nBytesToRead)

#define IOResource_Write(__self, __pChannel, __pBuffer, __nBytesToWrite) \
Object_InvokeN(write, IOResource, __self, __pChannel, __pBuffer, __nBytesToWrite)

#define IOResource_Seek(__self, __pChannel, __offset, __pOutPosition, __whence) \
Object_InvokeN(seek, IOResource, __self, __pChannel, __offset, __pOutPosition, __whence)

#define IOResource_Close(__self, __pChannel) \
Object_InvokeN(close, IOResource, __self, __pChannel)

#endif /* IOResource_h */
