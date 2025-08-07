//
//  Handler.h
//  kernel
//
//  Created by Dietmar Planitzer on 8/3/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef Handler_h
#define Handler_h

#include <stdarg.h>
#include <kern/kernlib.h>
#include <kobj/Object.h>


// A handler (or I/O handler) is an object that implements the policy that is
// used to regulate access to a driver and the interaction with a driver. Eg a
// handler controls how many I/O channels can be open at the same time, who can
// open an I/O channel and what operations are supported on a channel. Although
// most handlers sit on top of a driver instance, a handler is not required to
// interact with a driver. A handler may implement all mechanics on its own or
// employ the help of some other object.
open_class(Handler, Object,
    unsigned int    options;
);
open_class_funcs(Handler, Object,
    
    // Opens an I/O channel to the handler.
    // Override: Optional
    // Default Behavior: returns EPERM
    errno_t (*open)(void* _Nonnull _Locked self, unsigned int mode, intptr_t arg, IOChannelRef _Nullable * _Nonnull pOutChannel);

    // Closes the given I/O channel.
    // Override: Optional
    // Default Behavior: Does nothing and returns EOK
    errno_t (*close)(void* _Nonnull _Locked self, IOChannelRef _Nonnull pChannel);


    // Reads up to 'nBytesToRead' consecutive bytes from the underlying data
    // source and returns them in 'buf'. The actual amount of bytes read is
    // returned in 'nOutBytesRead' and returns a suitable status. Note that this
    // function will always either read at least one byte and return EOK or it
    // will read no bytes and return some error.
    // Override: Optional
    // Default Behavior: Returns EBADF
    errno_t (*read)(void* _Nonnull self, IOChannelRef _Nonnull ioc, void* _Nonnull buf, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead);

    // Writes up to 'nBytesToWrite' bytes from 'buf' to the underlying data source.
    // Override: Optional
    // Default Behavior: Returns EBADF
    errno_t (*write)(void* _Nonnull self, IOChannelRef _Nonnull ioc, const void* _Nonnull buf, ssize_t nBytesToWrite, ssize_t* _Nonnull nOutBytesWritten);


    // Sets the current position of the I/O channel 'ioc' based on 'offset' and
    // 'whence' and returns the previous position.
    // Override: Optional
    // Default Behavior: Updates the I/O channel's current position if
    //                   Handler_Seekable was specified; Returns EPIPE otherwise
    errno_t (*seek)(void* _Nonnull self, IOChannelRef _Nonnull ioc, off_t offset, off_t* _Nullable pOutOldPosition, int whence);
    
    // Returns the maximum allowable position for seeking. This position is also
    // known as the "end" of the medium. You must override this function if you
    // create the handler instance with the Handler_Seekable option.
    // Override: Optional
    // Default Behavior: Returns 0
    off_t (*getSeekableRange)(void* _Nonnull self);


    // Executes the handler specific function 'cmd' with command specific
    // arguments 'ap'.
    // Override: Optional
    // Default Behavior: Returns ENOTIOCTLCMD
    errno_t (*ioctl)(void* _Nonnull self, IOChannelRef _Nonnull ioc, int cmd, va_list ap);
);


#define Handler_Open(__self, __mode, __arg, __pOutChannel) \
invoke_n(open, Handler, __self, __mode, __arg, __pOutChannel)

#define Handler_Close(__self, __pChannel) \
invoke_n(close, Handler, __self, __pChannel)

#define Handler_Read(__self, __pChannel, __pBuffer, __nBytesToRead, __nOutBytesRead) \
invoke_n(read, Handler, __self, __pChannel, __pBuffer, __nBytesToRead, __nOutBytesRead)

#define Handler_Write(__self, __pChannel, __pBuffer, __nBytesToWrite, __nOutBytesWritten) \
invoke_n(write, Handler, __self, __pChannel, __pBuffer, __nBytesToWrite, __nOutBytesWritten)

#define Handler_Seek(__self, __pChannel, __offset, __pOutOldPosition, __whence) \
invoke_n(seek, Handler, __self, __pChannel, __offset, __pOutOldPosition, __whence)

#define Handler_vIoctl(__self, __chan, __cmd, __ap) \
invoke_n(ioctl, Handler, __self, __chan, __cmd, __ap)

extern errno_t Handler_Ioctl(HandlerRef _Nonnull self, IOChannelRef _Nonnull ioc, int cmd, ...);


//
// Subclassers
//


// Handler_Create() options
enum {
    kHandler_Seekable = 1,  // Handler should allow seeking
};

#define kHandler_OptionsMask    0x0f


extern errno_t Handler_Create(Class* _Nonnull pClass, unsigned options, HandlerRef _Nullable * _Nonnull pOutSelf);

#define Handler_GetSeekableRange(__self) \
invoke_0(getSeekableRange, Handler, __self)

#endif /* Handler_h */
