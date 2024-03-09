//
//  IOResource.h
//  kernel
//
//  Created by Dietmar Planitzer on 10/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef IOResource_h
#define IOResource_h

#include <klib/klib.h>
#include <filesystem/Inode.h>

CLASS_FORWARD(IOResource);


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: I/O Channel
////////////////////////////////////////////////////////////////////////////////

OPEN_CLASS_WITH_REF(IOChannel, Object,
    IOResourceRef _Nonnull  resource;
    unsigned int            mode;
);

typedef struct _IOChannelMethodTable {
    ObjectMethodTable   super;
    errno_t   (*dup)(void* _Nonnull self, IOChannelRef _Nullable * _Nonnull pOutChannel);
    errno_t   (*ioctl)(void* _Nonnull self, int cmd, va_list ap);
    errno_t   (*read)(void* _Nonnull self, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead);
    errno_t   (*write)(void* _Nonnull self, const void* _Nonnull pBuffer, ssize_t nBytesToWrite, ssize_t* _Nonnull nOutBytesWritten);
    errno_t   (*seek)(void* _Nonnull self, FileOffset offset, FileOffset* _Nullable pOutOldPosition, int whence);
    errno_t   (*close)(void* _Nonnull self);
} IOChannelMethodTable;


// I/O Channel functions for use by I/O channel users.

#define IOChannel_Dup(__self, __pOutChannel) \
Object_InvokeN(dup, IOChannel, __self, __pOutChannel)

extern errno_t IOChannel_vIOControl(IOChannelRef _Nonnull self, int cmd, va_list ap);

#define IOChannel_Read(__self, __pBuffer, __nBytesToRead, __nOutBytesRead) \
Object_InvokeN(read, IOChannel, __self, __pBuffer, __nBytesToRead, __nOutBytesRead)

#define IOChannel_Write(__self, __pBuffer, __nBytesToWrite, __nOutBytesWritten) \
Object_InvokeN(write, IOChannel, __self, __pBuffer, __nBytesToWrite, __nOutBytesWritten)

#define IOChannel_Seek(__self, __offset, __pOutOldPosition, __whence) \
Object_InvokeN(seek, IOChannel, __self, __offset, __pOutOldPosition, __whence)

#define IOChannel_Close(__self) \
Object_Invoke0(close, IOChannel, __self)


// I/O Channel functions for use by subclassers

// Creates an instance of an I/O channel. Subclassers should call this method in
// their own constructor implementation and then initialize the subclass specific
// properties. 
extern errno_t IOChannel_AbstractCreate(ClassRef _Nonnull pClass, IOResourceRef _Nonnull pResource, unsigned int mode, IOChannelRef _Nullable * _Nonnull pOutChannel);

// Creates a copy of the given I/O channel. Subclassers should call this in their
// own copying implementation and then copy the subclass specific properties.
extern errno_t IOChannel_AbstractCreateCopy(IOChannelRef _Nonnull pInChannel, IOChannelRef _Nullable * _Nonnull pOutChannel);

// Returns an *unowned* reference to the underlying resource. 
#define IOChannel_GetResource(__self) \
    ((IOChannelRef)__self)->resource

// Returns the I/O channel mode.
#define IOChannel_GetMode(__self) \
    ((IOChannelRef)__self)->mode
    

////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: I/O Resource
////////////////////////////////////////////////////////////////////////////////

OPEN_CLASS(IOResource, Object,);

#define SEEK_SET    0
#define SEEK_CUR    1
#define SEEK_END    2

typedef struct _IOResourceMethodTable {
    ObjectMethodTable   super;

    // Opens a resource context/channel to the resource. This new resource context
    // will be represented by a (file) descriptor in user space. The resource context
    // maintains state that is specific to this connection. This state will be
    // protected by the resource's internal locking mechanism. 'pNode' represents
    // the named resource instance that should be represented by the I/O channel. 
    errno_t   (*open)(void* _Nonnull self, InodeRef _Nonnull _Locked pNode, unsigned int mode, User user, IOChannelRef _Nullable * _Nonnull pOutChannel);

    // Creates an independent copy of the passed in I/O channel. Note that this function
    // is allowed to return a strong reference to the channel that was passed in if
    // the channel state is immutable. 
    errno_t   (*dup)(void* _Nonnull self, IOChannelRef _Nonnull pChannel, IOChannelRef _Nullable * _Nonnull pOutChannel);

    errno_t   (*read)(void* _Nonnull self, IOChannelRef _Nonnull pChannel, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nBytesRead);
    errno_t   (*write)(void* _Nonnull self, IOChannelRef _Nonnull pChannel, const void* _Nonnull pBuffer, ssize_t nBytesToWrite, ssize_t* _Nonnull nOutBytesWritten);

    // Executes the resource specific command 'cmd'.
    errno_t   (*ioctl)(void* _Nonnull self, int cmd, va_list ap);

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
    errno_t   (*close)(void* _Nonnull self, IOChannelRef _Nonnull pChannel);
} IOResourceMethodTable;

#define IOResource_Open(__self, __pNode, __options, __user, __pOutChannel) \
Object_InvokeN(open, IOResource, __self, __pNode, __options, __user, __pOutChannel)

#define IOResource_Dup(__self, __pChannel, __pOutChannel) \
Object_InvokeN(dup, IOResource, __self, __pChannel, __pOutChannel)

#define IOResource_Read(__self, __pChannel, __pBuffer, __nBytesToRead, __nOutBytesRead) \
Object_InvokeN(read, IOResource, __self, __pChannel, __pBuffer, __nBytesToRead, __nOutBytesRead)

#define IOResource_Write(__self, __pChannel, __pBuffer, __nBytesToWrite, __nOutBytesWritten) \
Object_InvokeN(write, IOResource, __self, __pChannel, __pBuffer, __nBytesToWrite, __nOutBytesWritten)

#define IOResource_vIOControl(__self, __cmd, __ap) \
Object_InvokeN(ioctl, IOResource, __self, __cmd, __ap)

#define IOResource_Close(__self, __pChannel) \
Object_InvokeN(close, IOResource, __self, __pChannel)

#endif /* IOResource_h */
