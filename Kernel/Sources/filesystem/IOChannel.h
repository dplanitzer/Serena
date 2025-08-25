//
//  IOChannel.h
//  kernel
//
//  Created by Dietmar Planitzer on 10/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef IOChannel_h
#define IOChannel_h

#include <stdarg.h>
#include <kobj/Any.h>
#include <kern/kernlib.h>
#include <kpi/fcntl.h>
#include <kpi/stat.h>
#include <sched/mtx.h>


// I/O channel ownership and operations tracking:
//
// I/O channels are reference counted objects. An I/O channel is created with one
// ownership reference active and a new ownership reference is established for a
// channel by calling IOChannel_Retain() on it. An ownership reference is released
// by calling IOChannel_Release(). Once the last ownership reference has been
// released and there are no ongoing I/O operations on the channel, and subject
// to the requirements of the I/O channel close mode (see below) the I/O channel
// is finalized. Finalizing an I/O channel means that it releases all its resources
// and that it may flush data that is still buffered up.
//
// Operations on an I/O channel are tracked with the IOChannel_BeginOperation()
// and IOChannel_EndOperation() calls. The former should be called before invoking
// one or more channel I/O operations and the latter one should be called at the
// end of a sequence of I/O operation calls.
//
// The IOChannelTable in a process takes care of the ownership of an I/O channel.
// It also provides the IOChannelTable_AcquireChannel() and
// IOChannelTable_RelinquishChannel() calls to take care of the I/O operation
// tracking.
//
// 
// Behavior of read, write, seek operations:
//
// I/O channels guarantee that these operations are serialized with respect to
// each other. This means that if you issue ie two concurrent write operations
// and both target the same byte range, that after the completion of each
// operation respective the byte range exclusively contains data provided by
// either operation and never a mix of data from both operations. This guarantee
// also includes that if you issue two overlapping concurrent operations that
// the one issued after the first one will start reading/writing at the file
// offset established by the completion of the previously issued operation.
//
//
// Behavior of the close() system call:
//
// The close() system call releases one ownership reference of an I/O channel
// and it removes the provided descriptor/ioc from the I/O channel table. The
// channel is scheduled for finalization once the last ownership reference (also
// meaning the last user visible descriptor/ioc) has been dropped.
// Close may flush buffered data to the I/O resource (ie disk). This write may
// fail with an error and close returns this error. However the close will still
// run to completion and close the I/O channel. The returned error is purely
// advisory and will not stop the close operation from closing the I/O channel.
//
//
// The three I/O channel close modes:
//
// 1) The close() system call removes an I/O channel ownership reference. If it
//    removes the last outstanding ownership reference then the I/O channel is
//    made invisible and the channel is scheduled for finalization. However the
//    actual finalization invocation is deferred until any still ongoing I/O
//    operations have completed. Once every ongoing I/O operation has completed
//    the channel is finalized.
//    (close with deferred finalization mode)
//
// 2) Similar to (1), however all ongoing I/O operations are canceled by the
//    last close() invocation and the I/O channel is finalized as soon as all
//    cancel operations have completed.
//    (canceling close mode)
//
// 3) Similar to (1) except that the last close() invocation is blocked until
//    all ongoing I/O operations have completed. Then the channel is finalized.
//    (blocking close mode)
//
// Only mode (1) is supported by the I/O channel class at this time. Support for
// the other modes is planned for the future.
open_class(IOChannel, Any,
    off_t       offset;         // I/O channel lock
    intptr_t    resource;       // Constant
    mtx_t       countLock;
    int32_t     ownerCount;     // countLock
    int32_t     useCount;       // countLock
    uint16_t    mode;           // Constant
    int16_t     channelType;    // Constant
);
any_subclass_funcs(IOChannel,

    //
    // Generic I/O Channel Interface
    //

    // Called once an I/O channel is ready to be deallocated for good. Overrides
    // should drain any still buffered data if this makes sense for the semantics
    // of the channel and it should then release all resources used by the channel.
    // This method may block on I/O operations.
    // This method may return an error. Note however that the error is purely for
    // informational purposes and that it may not stop the channel from completing
    // the finalization process. A channel is expected to be finalized and the
    // underlying I/O resource available for reuse once this method returns (with
    // or without an error).
    // Subclassers should not invoke the super implementation themselves. This is
    // taken care of automatically.
    errno_t (*finalize)(void* _Nonnull self);


    // Reads up to 'nBytesToRead' bytes of data from the (current position of the)
    // I/O channel and returns it in 'pBuffer'. An I/O channel may read less data
    // than request. The actual number of bytes read is returned in 'nOutBytesRead'.
    // If 0 is returned then the channel contains no more data. This is also known
    // as the end-of-file condition. If an error is encountered then a suitable
    // error code is returned and 'nOutBytesRead' is set to 0. An error condition
    // is only returned if a channel can not read at least one byte. If it can read
    // at least one byte then the number of bytes successfully read is returned
    // and no error code.
    errno_t (*read)(void* _Nonnull self, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead);

    // Writes up to 'nBytesToWrite' bytes to the I/O channel. Works similar to
    // how read() works.
    errno_t (*write)(void* _Nonnull self, const void* _Nonnull pBuffer, ssize_t nBytesToWrite, ssize_t* _Nonnull nOutBytesWritten);

    // Sets the current file position of an I/O channel. Returns ESPIPE by
    // default. A channel that supports seeking should override this method and
    // lock the channel and then invoke IOChannel_DoSeek().
    errno_t (*seek)(void* _Nonnull self, off_t offset, off_t* _Nullable pOutOldPosition, int whence);

    // Invoked by seek() to get the size of the seekable space. The maximum
    // position to which a client is allowed to seek is this value minus one.
    // Override: Optional
    // Default Behavior: Returns 0
    off_t (*getSeekableRange)(void* _Nonnull _Locked self);


    // Execute an I/O channel specific command.
    errno_t (*ioctl)(void* _Nonnull self, int cmd, va_list ap);


    //
    // Inode Channel Interface
    //

    // Returns information about the Inode to which the channel is connected if
    // the channel is an Inode channel. Returns EBADF otherwise.
    // Override: Optional
    // Default Behavior: Returns EBADF
    errno_t (*getFileInfo)(void* _Nonnull self, struct stat* _Nonnull pOutInfo);

    // Reduces or increases the size of a regular file if the channel is connected
    // to an Inode. Returns EBADF otherwise
    // Override: Optional
    // Default Behavior: Returns EBADF
    errno_t (*truncate)(void* _Nonnull self, off_t length);
);


// Returns the I/O channel type.
#define IOChannel_GetChannelType(/*_Nonnull*/ __self) \
((int)((IOChannelRef)(__self))->channelType)

// Returns the I/O channel mode.
#define IOChannel_GetMode(/*_Nonnull*/ __self) \
((unsigned int)((IOChannelRef)(__self))->mode)

// Returns true if the I/O channel is readable.
#define IOChannel_IsReadable(__self) \
((((IOChannelRef)__self)->mode & O_RDONLY) == O_RDONLY)

// Returns true if the I/O channel is writable.
#define IOChannel_IsWritable(__self) \
((((IOChannelRef)__self)->mode & O_WRONLY) == O_WRONLY)


// Returns the resource to which the I/O channel is connected
#define IOChannel_GetResource(__self) \
((IOChannelRef)__self)->resource

#define IOChannel_GetResourceAs(__self, __class) \
((__class##Ref) ((IOChannelRef)__self)->resource)


// Returns the current seek position
#define IOChannel_GetOffset(/*_Nonnull _Locked*/ __self) \
((IOChannelRef)(__self))->offset

// Sets the current seek position
#define IOChannel_SetOffset(/*_Nonnull _Locked*/ __self, __pos) \
((IOChannelRef)(__self))->offset = (__pos)

// Increment the current seek position by the give signed value
#define IOChannel_IncrementOffsetBy(/*_Nonnull _Locked*/ __self, __delta) \
((IOChannelRef)(__self))->offset += (__delta)


extern errno_t IOChannel_vFcntl(IOChannelRef _Nonnull self, int cmd, int* _Nonnull pResult, va_list ap);


#define IOChannel_Read(__self, __pBuffer, __nBytesToRead, __nOutBytesRead) \
invoke_n(read, IOChannel, __self, __pBuffer, __nBytesToRead, __nOutBytesRead)

#define IOChannel_Write(__self, __pBuffer, __nBytesToWrite, __nOutBytesWritten) \
invoke_n(write, IOChannel, __self, __pBuffer, __nBytesToWrite, __nOutBytesWritten)

#define IOChannel_Seek(__self, __offset, __pOutNewPos, __whence) \
invoke_n(seek, IOChannel, __self, __offset, __pOutNewPos, __whence)

#define IOChannel_vIoctl(__self, __cmd, __ap) \
invoke_n(ioctl, IOChannel, __self, __cmd, __ap)

extern errno_t IOChannel_Ioctl(IOChannelRef _Nonnull self, int cmd, ...);


#define IOChannel_GetFileInfo(__self, __pOutInfo) \
invoke_n(getFileInfo, IOChannel, __self, __pOutInfo)

#define IOChannel_Truncate(__self, __length) \
invoke_n(truncate, IOChannel, __self, __length)


extern IOChannelRef IOChannel_Retain(IOChannelRef _Nonnull self);

#define IOChannel_RetainAs(__pChannel, __type) \
    ((__type)IOChannel_Retain((IOChannelRef)__pChannel))

extern errno_t IOChannel_Release(IOChannelRef _Nullable self);


//
// Subclassers
//

// Creates an instance of an I/O channel. Subclassers should call this method in
// their own constructor implementation and then initialize the subclass specific
// properties. 
extern errno_t IOChannel_Create(Class* _Nonnull pClass, int channelType, unsigned int mode, intptr_t resource, IOChannelRef _Nullable * _Nonnull pOutChannel);

// Implements the actual seek logic. Invokes IOChannel_GetSeekableRange() if
// needed to get the range over which seeking is supported.
extern errno_t IOChannel_DoSeek(IOChannelRef _Nonnull self, off_t offset, off_t* _Nullable pOutNewPos, int whence);

// Returns the size of the seekable range
#define IOChannel_GetSeekableRange(__self) \
invoke_0(getSeekableRange, IOChannel, __self)


//
// For use by IOChannelTable
//

extern void IOChannel_BeginOperation(IOChannelRef _Nonnull self);
extern void IOChannel_EndOperation(IOChannelRef _Nonnull self);

#endif /* IOChannel_h */
