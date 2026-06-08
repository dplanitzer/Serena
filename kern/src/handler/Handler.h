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


//XXX add documentation
open_class(Handler, Object,
    off_t       offset;         // handler lock
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
    // lock the channel and then invoke Handler_DoSeek().
    errno_t (*seek)(void* _Nonnull self, off_t offset, off_t* _Nullable pOutOldPosition, int whence);

    // Invoked by seek() to get the size of the seekable space. The maximum
    // position to which a client is allowed to seek is this value minus one.
    // Override: Optional
    // Default Behavior: Returns 0
    off_t (*getSeekableRange)(void* _Nonnull _Locked self);


    errno_t (*shutdown)(void* _Nonnull self);
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

extern errno_t Handler_GetInfo(HandlerRef _Nonnull self, int flavor, fd_info_ref _Nonnull info);


#define Handler_Shutdown(__self) \
invoke_0(shutdown, Handler, __self)


//
// Subclassers
//

// Creates an instance of an handler. Subclassers should call this method in
// their own constructor implementation and then initialize the subclass specific
// properties. 
extern errno_t Handler_Create(Class* _Nonnull pClass, int type, unsigned int mode, HandlerRef _Nullable * _Nonnull pOutHandler);

// Implements the actual seek logic. Invokes Handler_GetSeekableRange() if
// needed to get the range over which seeking is supported.
extern errno_t Handler_DoSeek(HandlerRef _Nonnull self, off_t offset, off_t* _Nullable pOutNewPos, int whence);

// Returns the size of the seekable range
#define Handler_GetSeekableRange(__self) \
invoke_0(getSeekableRange, Handler, __self)


//
// HandlerTable
//

#define Handler_IncrementDescriptorCount(__self) \
atomic_int_fetch_add(&((HandlerRef)__self)->descriptorCount, 1)

#define Handler_DecrementDescriptorCount(__self) \
atomic_int_fetch_sub(&((HandlerRef)__self)->descriptorCount, 1)

#endif /* Handler_h */
