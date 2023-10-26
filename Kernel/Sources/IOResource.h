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

CLASS_FORWARD(IOChannel);
CLASS_FORWARD(IOResource);


CLASS_INTERFACE(IOChannel, Object,
    IOResourceRef _Nonnull  resource;
    UInt                    options;
    Byte                    state[1];
);
enum IOChannelMethodIndex {
    kIOChannelMethodIndex_read = kObjectMethodIndex_deinit + 1,
    kIOChannelMethodIndex_write,
    kIOChannelMethodIndex_close,

    kIOChannelMethodIndex_Count = kIOChannelMethodIndex_close + 1
};
INSTANCE_METHOD_N(ByteCount, IOChannel, read, Byte* _Nonnull pBuffer, ByteCount nBytesToRead);
INSTANCE_METHOD_N(ByteCount, IOChannel, write, const Byte* _Nonnull pBuffer, ByteCount nBytesToWrite);
INSTANCE_METHOD_0(ErrorCode, IOChannel, close);

#define FREAD   0x0001
#define FWRITE  0x0002


extern ErrorCode IOChannel_Create(IOResourceRef _Nonnull pResource, UInt options, ByteCount stateSize, IOChannelRef _Nullable * _Nonnull pOutChannel);

// Creates a copy of the given rescon with a state size of 'stateSize'.
extern ErrorCode IOChannel_CreateCopy(IOChannelRef _Nonnull pInChannel, ByteCount stateSize, IOChannelRef _Nullable * _Nonnull pOutChannel);

#define IOChannel_GetStateAs(__self, __type) \
    ((__type*) &__self->state[0])

#define IOChannel_Read(__self, __pBuffer, __nBytesToRead) \
Object_Invoke(read, IOChannel, __self, __pBuffer, __nBytesToRead)

#define IOChannel_Write(__self, __pBuffer, __nBytesToWrite) \
Object_Invoke(write, IOChannel, __self, __pBuffer, __nBytesToWrite)

#define IOChannel_Close(__self) \
Object_Invoke(read, IOChannel, __self)


////////////////////////////////////////////////////////////////////////////////

CLASS_INTERFACE(IOResource, Object,);
enum IOResourceMethodIndex {
    kIOResourceMethodIndex_open = kObjectMethodIndex_deinit + 1,
    kIOResourceMethodIndex_dup,
    kIOResourceMethodIndex_command,
    kIOResourceMethodIndex_read,
    kIOResourceMethodIndex_write,
    kIOResourceMethodIndex_close,

    kIOResourceMethodIndex_Count = kIOResourceMethodIndex_close + 1
};


// Opens a resource context/channel to the resource. This new resource context
// will be represented by a (file) descriptor in user space. The resource context
// maintains state that is specific to this connection. This state will be
// protected by the resource's internal locking mechanism.
INSTANCE_METHOD_N(ErrorCode, IOResource, open, const Character* _Nonnull pPath, UInt options, IOChannelRef _Nullable * _Nonnull pOutChannel);

// Creates an independent copy of the passed in rescon. Note that this function
// is allowed to return a strong reference to the rescon that was passed in if
// the rescon state is immutable. 
INSTANCE_METHOD_N(ErrorCode, IOResource, dup, IOChannelRef _Nonnull pChannel, IOChannelRef _Nullable * _Nonnull pOutChannel);

// Executes the resource specific command 'op'.
INSTANCE_METHOD_N(ErrorCode, IOResource, command, Int op, va_list ap);

INSTANCE_METHOD_N(ByteCount, IOResource, read, void* _Nonnull pContext, Byte* _Nonnull pBuffer, ByteCount nBytesToRead);
INSTANCE_METHOD_N(ByteCount, IOResource, write, void* _Nonnull pContext, const Byte* _Nonnull pBuffer, ByteCount nBytesToWrite);

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
INSTANCE_METHOD_N(ErrorCode, IOResource, close, void* _Nonnull pContext);


#define IOResource_Open(__self, __pPath, __options, __pOutChannel) \
Object_Invoke(open, IOResource, __self, __pPath, __options, __pOutChannel)

#define IOResource_Dup(__self, __pChannel, __pOutChannel) \
Object_Invoke(dup, IOResource, __self, __pChannel, __pOutChannel)

#define IOResource_Close(__self, __pChannel) \
Object_Invoke(close, IOResource, __self, __pChannel)

#define IOResource_Command(__self, __op, __ap) \
Object_Invoke(command, IOResource, __self, __op, __ap)

#define IOResource_Read(__self, __pChannel, __pBuffer, __nBytesToRead) \
Object_Invoke(read, IOResource, __self, __pChannel, __pBuffer, __nBytesToRead)

#define IOResource_Write(__self, __pChannel, __pBuffer, __nBytesToWrite) \
Object_Invoke(write, IOResource, __self, __pChannel, __pBuffer, __nBytesToWrite)
    
#endif /* IOResource_h */
