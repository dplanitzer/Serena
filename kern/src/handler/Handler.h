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
#include <ext/atomic.h>
#include <ext/try.h>
#include <kobj/Object.h>
#include <kern/kernlib.h>
#include <kpi/attr.h>
#include <kpi/fd.h>


// A handler object acts as an adaptor that allows the file descriptor API to
// interact with some kind of in-kernel object. E.g. a file, driver instance,
// pipe, etc.
//
// The handler base class does not implement any operations. It expects that
// subclasses implement the logic of making the read, write, seek and close
// operations work. A particular handler subclass may add more operations.
//
// The semantic expectations of a handler are:
// - multiple operations may be started in parallel and run in parallel. However
//   a handler subclass may impose limits on the kind of operations that can run
//   in parallel and the number of operations that can run in parallel.
//
// - Once Handler_Close() has returned, all operations that were still in-flight
//   at the time Handler_Close() was called have finished running or have been
//   cancelled and no new operations can be started anymore. An attempt to start
//   a new operation will result in a EBADF error.
//
open_class(Handler, Object,
    atomic_int  descriptorCount;
    uint16_t    mode;           // Constant
    int16_t     type;           // Constant
);
open_class_funcs(Handler, Object,

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
    // lock the channel and then invoke do_seek().
    errno_t (*seek)(void* _Nonnull self, off_t offset, off_t* _Nullable pOutOldPosition, int whence);


    // Closes the handler. This function guarantees that no more operations are
    // active on the handler and no new operations can be started on the handler
    // anymore once it returns to the caller. This function may return an error.
    // Note that this error is advisory. Meaning the handler is closed no matter
    // whether EOK or an error code is returned once this function returns to
    // the caller.
    // Subclassers must ensure that:
    // - either the close function blocks until all currently active operations
    //   have finished successfully or with an error.
    // - or the close function cancels all currently active operations and blocks
    //   until the cancel has finished.
    //
    // It is imperative that close() only returns after no operations are running
    // anymore and the handler has been atomically marked as closed to ensure that
    // no new operations can be started anymore after return from close().
    errno_t (*close)(void* _Nonnull self);
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
extern errno_t Handler_SetFlags(HandlerRef _Nonnull self, int op, int flags);


#define Handler_Read(__self, __pBuffer, __nBytesToRead, __nOutBytesRead) \
invoke_n(read, Handler, __self, __pBuffer, __nBytesToRead, __nOutBytesRead)

#define Handler_Write(__self, __pBuffer, __nBytesToWrite, __nOutBytesWritten) \
invoke_n(write, Handler, __self, __pBuffer, __nBytesToWrite, __nOutBytesWritten)

#define Handler_Seek(__self, __offset, __pOutNewPos, __whence) \
invoke_n(seek, Handler, __self, __offset, __pOutNewPos, __whence)

extern errno_t Handler_GetInfo(HandlerRef _Nonnull self, int flavor, fd_info_ref _Nonnull info);


#define Handler_Close(__self) \
invoke_0(close, Handler, __self)


//
// Subclassers
//

// Creates an instance of an handler. Subclassers should call this method in
// their own constructor implementation and then initialize the subclass specific
// properties. 
extern errno_t Handler_Create(Class* _Nonnull pClass, int type, unsigned int mode, HandlerRef _Nullable * _Nonnull pOutHandler);

// Implements the actual seek logic. 'endPos' is the end position of the
// seekable space. This typically corresponds to the length of the seekable
// space. This parameter is only needed if 'whence' == SEEK_END. 
extern errno_t do_seek(off_t offset, int whence, off_t endPos, off_t* _Nonnull pos);


//
// HandlerTable
//

#define Handler_IncrementDescriptorCount(__self) \
atomic_int_fetch_add(&((HandlerRef)__self)->descriptorCount, 1)

#define Handler_DecrementDescriptorCount(__self) \
atomic_int_fetch_sub(&((HandlerRef)__self)->descriptorCount, 1)

#endif /* Handler_h */
