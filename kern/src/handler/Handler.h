//
//  Handler.h
//  kernel
//
//  Created by Dietmar Planitzer on 10/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef Handler_h
#define Handler_h

#include <stdarg.h>
#include <ext/try.h>
#include <kobj/Any.h>
#include <kern/kernlib.h>
#include <kpi/attr.h>
#include <kpi/fd.h>
#include <sched/mtx.h>


// Handler ownership and operations tracking:
//
// Handlers are reference counted objects. A handler is created with one
// ownership reference active and a new ownership reference is established for a
// channel by calling Handler_Retain() on it. An ownership reference is released
// by calling Handler_Release(). Once the last ownership reference has been
// released and there are no ongoing I/O operations on the channel, and subject
// to the requirements of the handler close mode (see below) the handler
// is finalized. Finalizing an handler means that it releases all its resources
// and that it may flush data that is still buffered up.
//
// Operations on an handler are tracked with the Handler_BeginOperation()
// and Handler_EndOperation() calls. The former should be called before invoking
// one or more channel I/O operations and the latter one should be called at the
// end of a sequence of I/O operation calls.
//
// The HandlerTable in a process takes care of the ownership of an handler.
// It also provides the HandlerTable_AcquireHandler() and
// Handler_EndOperation() calls to take care of the I/O operation tracking.
//
// 
// Behavior of read, write, seek operations:
//
// Handlers guarantee that these operations are serialized with respect to
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
// The close() system call releases one ownership reference of an handler
// and it removes the provided descriptor/ioc from the handler table. The
// channel is scheduled for finalization once the last ownership reference (also
// meaning the last user visible descriptor/ioc) has been dropped.
// Close may flush buffered data to the I/O resource (ie disk). This write may
// fail with an error and close returns this error. However the close will still
// run to completion and close the handler. The returned error is purely
// advisory and will not stop the close operation from closing the handler.
//
//
// The three handler close modes:
//
// 1) The close() system call removes an handler ownership reference. If it
//    removes the last outstanding ownership reference then the handler is
//    made invisible and the channel is scheduled for finalization. However the
//    actual finalization invocation is deferred until any still ongoing I/O
//    operations have completed. Once every ongoing I/O operation has completed
//    the channel is finalized.
//    (close with deferred finalization mode)
//
// 2) Similar to (1), however all ongoing I/O operations are canceled by the
//    last close() invocation and the handler is finalized as soon as all
//    cancel operations have completed.
//    (canceling close mode)
//
// 3) Similar to (1) except that the last close() invocation is blocked until
//    all ongoing I/O operations have completed. Then the channel is finalized.
//    (blocking close mode)
//
// Only mode (1) is supported by the handler class at this time. Support for
// the other modes is planned for the future.
open_class(Handler, Any,
    off_t       offset;         // handler lock
    intptr_t    resource;       // Constant
    mtx_t       countLock;
    int32_t     ownerCount;     // countLock
    int32_t     useCount;       // countLock
    uint16_t    mode;           // Constant
    int16_t     type;           // Constant
);
any_subclass_funcs(Handler,

    //
    // Generic handler Interface
    //

    // Called once an handler is ready to be deallocated for good. Overrides
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
    // handler and returns it in 'pBuffer'. An handler may read less data
    // than request. The actual number of bytes read is returned in 'nOutBytesRead'.
    // If 0 is returned then the channel contains no more data. This is also known
    // as the end-of-file condition. If an error is encountered then a suitable
    // error code is returned and 'nOutBytesRead' is set to 0. An error condition
    // is only returned if a channel can not read at least one byte. If it can read
    // at least one byte then the number of bytes successfully read is returned
    // and no error code.
    errno_t (*read)(void* _Nonnull self, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead);

    // Writes up to 'nBytesToWrite' bytes to the handler. Works similar to
    // how read() works.
    errno_t (*write)(void* _Nonnull self, const void* _Nonnull pBuffer, ssize_t nBytesToWrite, ssize_t* _Nonnull nOutBytesWritten);

    // Sets the current file position of an handler. Returns ESPIPE by
    // default. A channel that supports seeking should override this method and
    // lock the channel and then invoke Handler_DoSeek().
    errno_t (*seek)(void* _Nonnull self, off_t offset, off_t* _Nullable pOutOldPosition, int whence);

    // Invoked by seek() to get the size of the seekable space. The maximum
    // position to which a client is allowed to seek is this value minus one.
    // Override: Optional
    // Default Behavior: Returns 0
    off_t (*getSeekableRange)(void* _Nonnull _Locked self);


    // Execute an handler specific command.
    errno_t (*ioctl)(void* _Nonnull self, int cmd, va_list ap);


    //
    // Inode Handler Interface
    //

    // Returns the attributes of the Inode to which the channel is connected if
    // the channel is an Inode channel. Returns EBADF otherwise.
    // Override: Optional
    // Default Behavior: Returns EBADF
    errno_t (*getAttributes)(void* _Nonnull self, fs_attr_t* _Nonnull attr);

    // Reduces or increases the size of a regular file if the channel is connected
    // to an Inode. Returns EBADF otherwise
    // Override: Optional
    // Default Behavior: Returns EBADF
    errno_t (*truncate)(void* _Nonnull self, off_t length);
);


// Returns the handler type.
#define Handler_GetType(/*_Nonnull*/ __self) \
((int)((HandlerRef)(__self))->type)

// Returns the handler mode.
#define Handler_GetMode(/*_Nonnull*/ __self) \
((unsigned int)((HandlerRef)(__self))->mode)

// Returns true if the handler is readable.
#define Handler_IsReadable(__self) \
((((HandlerRef)__self)->mode & O_RDONLY) == O_RDONLY)

// Returns true if the handler is writable.
#define Handler_IsWritable(__self) \
((((HandlerRef)__self)->mode & O_WRONLY) == O_WRONLY)

// Returns a copy of the flags
extern int Handler_GetFlags(HandlerRef _Nonnull self);


// Returns the resource to which the handler is connected
#define Handler_GetResource(__self) \
((HandlerRef)__self)->resource

#define Handler_GetResourceAs(__self, __class) \
((__class##Ref) ((HandlerRef)__self)->resource)


// Returns the current seek position
#define Handler_GetOffset(/*_Nonnull _Locked*/ __self) \
((HandlerRef)(__self))->offset

// Sets the current seek position
#define Handler_SetOffset(/*_Nonnull _Locked*/ __self, __pos) \
((HandlerRef)(__self))->offset = (__pos)

// Increment the current seek position by the give signed value
#define Handler_IncrementOffsetBy(/*_Nonnull _Locked*/ __self, __delta) \
((HandlerRef)(__self))->offset += (__delta)


extern errno_t Handler_SetFlags(HandlerRef _Nonnull self, int op, int flags);


#define Handler_Read(__self, __pBuffer, __nBytesToRead, __nOutBytesRead) \
invoke_n(read, Handler, __self, __pBuffer, __nBytesToRead, __nOutBytesRead)

#define Handler_Write(__self, __pBuffer, __nBytesToWrite, __nOutBytesWritten) \
invoke_n(write, Handler, __self, __pBuffer, __nBytesToWrite, __nOutBytesWritten)

#define Handler_Seek(__self, __offset, __pOutNewPos, __whence) \
invoke_n(seek, Handler, __self, __offset, __pOutNewPos, __whence)

#define Handler_vIoctl(__self, __cmd, __ap) \
invoke_n(ioctl, Handler, __self, __cmd, __ap)

extern errno_t Handler_Ioctl(HandlerRef _Nonnull self, int cmd, ...);
extern errno_t Handler_GetInfo(HandlerRef _Nonnull self, int flavor, fd_info_ref _Nonnull info);


#define Handler_GetAttributes(__self, __attr) \
invoke_n(getAttributes, Handler, __self, __attr)

#define Handler_Truncate(__self, __length) \
invoke_n(truncate, Handler, __self, __length)


extern HandlerRef Handler_Retain(HandlerRef _Nonnull self);

#define Handler_RetainAs(__hnd, __type) \
    ((__type)Handler_Retain((HandlerRef)__hnd))

extern errno_t Handler_Release(HandlerRef _Nullable self);


//
// Subclassers
//

// Creates an instance of an handler. Subclassers should call this method in
// their own constructor implementation and then initialize the subclass specific
// properties. 
extern errno_t Handler_Create(Class* _Nonnull pClass, int type, unsigned int mode, intptr_t resource, HandlerRef _Nullable * _Nonnull pOutHandler);

// Implements the actual seek logic. Invokes Handler_GetSeekableRange() if
// needed to get the range over which seeking is supported.
extern errno_t Handler_DoSeek(HandlerRef _Nonnull self, off_t offset, off_t* _Nullable pOutNewPos, int whence);

// Returns the size of the seekable range
#define Handler_GetSeekableRange(__self) \
invoke_0(getSeekableRange, Handler, __self)


//
// For use by HandlerTable
//

extern void Handler_BeginOperation(HandlerRef _Nonnull self);
extern void Handler_EndOperation(HandlerRef _Nonnull self);

#endif /* Handler_h */
