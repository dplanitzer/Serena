//
//  IOChannel.h
//  kernel
//
//  Created by Dietmar Planitzer on 10/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef IOChannel_h
#define IOChannel_h

#include <klib/klib.h>
#include <System/File.h>


OPEN_CLASS_WITH_REF(IOChannel, Object,
    unsigned int    mode;
);
typedef struct IOChannelMethodTable {
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

#define IOChannel_vIOControl(__self, __cmd, __ap) \
Object_InvokeN(ioctl, IOChannel, __self, __cmd, __ap)

extern errno_t IOChannel_Read(IOChannelRef _Nonnull self, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead);

extern errno_t IOChannel_Write(IOChannelRef _Nonnull self, const void* _Nonnull pBuffer, ssize_t nBytesToWrite, ssize_t* _Nonnull nOutBytesWritten);

#define IOChannel_Seek(__self, __offset, __pOutOldPosition, __whence) \
Object_InvokeN(seek, IOChannel, __self, __offset, __pOutOldPosition, __whence)

#define IOChannel_Close(__self) \
Object_Invoke0(close, IOChannel, __self)


// I/O Channel functions for use by subclassers

// Creates an instance of an I/O channel. Subclassers should call this method in
// their own constructor implementation and then initialize the subclass specific
// properties. 
extern errno_t IOChannel_AbstractCreate(ClassRef _Nonnull pClass, unsigned int mode, IOChannelRef _Nullable * _Nonnull pOutChannel);

// Creates a copy of the given I/O channel. Subclassers should call this in their
// own copying implementation and then copy the subclass specific properties.
extern errno_t IOChannel_AbstractCreateCopy(IOChannelRef _Nonnull pInChannel, IOChannelRef _Nullable * _Nonnull pOutChannel);

// Returns the I/O channel mode.
#define IOChannel_GetMode(__self) \
    ((IOChannelRef)__self)->mode

#endif /* IOChannel_h */
