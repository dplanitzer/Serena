//
//  Handler.h
//  kernel
//
//  Created by Dietmar Planitzer on 10/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef Handler_h
#define Handler_h

#include <ext/atomic.h>
#include <ext/try.h>
#include <kobj/Object.h>
#include <kpi/fd.h>
#include <kpi/types.h>
#include <kpi/_seek.h>


// A handler object acts as a bridge between a user space descriptor and the
// corresponding in-kernel object. E.g. a file, driver instance, pipe, etc.
// Thus a handler can assume that its I/O functions will always be invoked on
// behalf of a user space process and that the kernel will never create a
// handler for use inside the kernel.
//
// The handler base class does not implement any operations. It expects that
// subclasses implement the logic of making the read, write, seek, etc
// operations work. A particular handler subclass may add more operations.
//
// The semantic expectations of a handler are:
// - multiple operations may be started in parallel and run in parallel. However
//   a handler subclass may impose limits on the kind of operations that can run
//   in parallel and the number of operations that can run in parallel.
//
// - the implementation of each operation (read, write, etc) has to call
//   Handler_GetFlags() *once at the beginning* and then act on the flags snapshot
//   that it has received. The flags shall be kept unmodified throughout the
//   execution of the operation.
//
// - Code which wants to issue an operation on a handler has to hold a strong
//   reference to the handler and has to maintain it until the operation has
//   returned.
//
// - a handler is "closed" when the last strong reference to it is dropped.
//
open_class(Handler, Object,
    atomic_int  flags;
    int         type;           // Constant
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
);


// Returns the handler type.
#define Handler_GetType(/*_Nonnull*/ __self) \
(((HandlerRef)(__self))->type)


// Returns the handler flags that were passed to it at open() time.
#define Handler_GetFlags(/*_Nonnull*/ __self) \
((unsigned int)atomic_int_load(&((HandlerRef)(__self))->flags))

// Updates the handler flags. Note that only a subset of the flags are modifiable.
extern errno_t Handler_SetFlags(HandlerRef _Nonnull self, int op, int flags);


#define Handler_Read(__self, __pBuffer, __nBytesToRead, __nOutBytesRead) \
invoke_n(read, Handler, __self, __pBuffer, __nBytesToRead, __nOutBytesRead)

#define Handler_Write(__self, __pBuffer, __nBytesToWrite, __nOutBytesWritten) \
invoke_n(write, Handler, __self, __pBuffer, __nBytesToWrite, __nOutBytesWritten)

#define Handler_Seek(__self, __offset, __pOutNewPos, __whence) \
invoke_n(seek, Handler, __self, __offset, __pOutNewPos, __whence)

extern errno_t Handler_GetInfo(HandlerRef _Nonnull self, int flavor, fd_info_ref _Nonnull info);


//
// Subclassers
//

// Creates an instance of an handler. Subclassers should call this method in
// their own constructor implementation and then initialize the subclass specific
// properties. 
extern errno_t Handler_Create(Class* _Nonnull pClass, int type, fd_flags_t oflags, HandlerRef _Nullable * _Nonnull pOutHandler);

// Implements the actual seek logic. 'endPos' is the end position of the
// seekable space. This typically corresponds to the length of the seekable
// space. This parameter is only needed if 'whence' == SEEK_END. 
extern errno_t do_seek(off_t offset, int whence, off_t endPos, off_t* _Nonnull pos);

#endif /* Handler_h */
