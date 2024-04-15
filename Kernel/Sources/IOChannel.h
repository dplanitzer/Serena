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
#include <kobj/Any.h>
#include <dispatcher/Lock.h>
#include <System/File.h>
#include <System/IOChannel.h>


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
// Operations on an I/O channel are tracking with the IOChannel_BeginOperation()
// and IOChannel_EndOperation() calls. The former should be called before invoking
// one or more channel I/O operations and the latter one should be called at the
// end of a sequence of I/O operation calls.
//
// The IOChannelTable in a process takes care of the ownership of an I/O channel.
// It also offers the IOChannelTable_AcquireChannel() and
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
//    (deferred close mode)
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
open_class_with_ref(IOChannel, Any,
    Lock            countLock;
    int32_t         ownerCount;
    int32_t         useCount;
    unsigned int    mode;       // Constant over the lifetime of a channel
);
any_subclass_funcs(IOChannel,
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

    // Creates a copy of the receiver. Copying an I/O channel means that the new
    // channel should be equipped with an independent copy of the channel state.
    // However the underlying I/O resource should typically not be copied and
    // instead it should be shared between teh channel. Ie copying a file channel
    // means that the current file offset, channel open mode, etc is copied but
    // the original channel and the copied channel will share the underlying
    // file object.
    // All that said, it may sometimes be appropriate for a channel to copy the
    // underlying I/O resource too.
    errno_t (*copy)(void* _Nonnull self, IOChannelRef _Nullable * _Nonnull pOutChannel);

    // Execute an I/O channel specific command.
    errno_t (*ioctl)(void* _Nonnull self, int cmd, va_list ap);

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

    // Sets the current file position of an I/O channel. A channel which doesn't
    // support seeking will return ESPIPE and 0 as the old position. The next
    // channel read/write operation will start reading/writing from this
    // position.
    errno_t (*seek)(void* _Nonnull self, FileOffset offset, FileOffset* _Nullable pOutOldPosition, int whence);
);


// I/O Channel functions for use by I/O channel users.

#define IOChannel_Copy(__self, __pOutChannel) \
invoke_n(copy, IOChannel, __self, __pOutChannel)

#define IOChannel_vIOControl(__self, __cmd, __ap) \
invoke_n(ioctl, IOChannel, __self, __cmd, __ap)

extern errno_t IOChannel_Read(IOChannelRef _Nonnull self, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead);

extern errno_t IOChannel_Write(IOChannelRef _Nonnull self, const void* _Nonnull pBuffer, ssize_t nBytesToWrite, ssize_t* _Nonnull nOutBytesWritten);

#define IOChannel_Seek(__self, __offset, __pOutOldPosition, __whence) \
invoke_n(seek, IOChannel, __self, __offset, __pOutOldPosition, __whence)


// I/O Channel functions for use by subclassers

// Creates an instance of an I/O channel. Subclassers should call this method in
// their own constructor implementation and then initialize the subclass specific
// properties. 
extern errno_t IOChannel_AbstractCreate(Class* _Nonnull pClass, unsigned int mode, IOChannelRef _Nullable * _Nonnull pOutChannel);

// Returns the I/O channel mode.
#define IOChannel_GetMode(__self) \
    ((IOChannelRef)__self)->mode


// I/O channel functions for use by IOChannelTable

extern void IOChannel_Retain(IOChannelRef _Nonnull self);
extern errno_t IOChannel_Release(IOChannelRef _Nullable self);

extern void IOChannel_BeginOperation(IOChannelRef _Nonnull self);
extern void IOChannel_EndOperation(IOChannelRef _Nonnull self);

#endif /* IOChannel_h */
