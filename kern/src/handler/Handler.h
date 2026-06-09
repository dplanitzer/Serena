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
// - the implementation of each operation (read, write, etc) has to call
//   Handler_GetMode() *once at the beginning* and then act on the mode snapshot
//   that it has received. The mode shall be kept static throughout the execution
//   of the operation.
//
// - Code which wants to issue an operation on a handler has to hold a strong
//   reference to the handler and has to maintain it until the operation has
//   returned.
//
// - Note that it is up to a specific handler implementation to decide whether
//   Handler_Close() cancels all or some of the currently executing operations.
//   Further note that an operation that was started before Handler_Close() is
//   called by some other vcpu may continue to run after Handler_Close() returns.
//   The handler will only be freed once the last strong reference has been
//   released and any code that is calling a handler function must maintain a
//   strong reference until after the handler function has returned.
//
open_class(Handler, Object,
    atomic_int  descriptorCount;
    atomic_int  mode;
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


    // Closes the handler. A close function may mark the handler as closed such
    // that no new operations can be started after close() has returned. However
    // this is not required. Note that the way handlers and file descriptors
    // work together, it is guaranteed that user space code can not issue any
    // new operations on a descriptor once Handler_Close() has been called on
    // the handler associated with the descriptor (the descriptor and thus
    // handler is removed from the table before Handler_Close() is called).
    errno_t (*close)(void* _Nonnull self);
);


// Returns the handler type.
#define Handler_GetType(/*_Nonnull*/ __self) \
(((HandlerRef)(__self))->type)

// Returns the handler mode.
#define Handler_GetMode(/*_Nonnull*/ __self) \
((unsigned int)atomic_int_load(&((HandlerRef)(__self))->mode))

// Returns a copy of the flags. The flags are the modifiable subset of mode bits.
#define Handler_GetFlags(/*_Nonnull*/ __self) \
(Handler_GetMode(__self) & O_FLAGS)

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
