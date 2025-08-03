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


// driver for the CD-ROM drive.
open_class(Handler, Object,
    did_t   id;
);
open_class_funcs(Handler, Object,
    
    // Invoked as the result of calling Driver_Open(). A driver subclass may
    // override this method to create a driver specific I/O channel object. The
    // default implementation creates a DriverChannel instance which supports
    // read, write, ioctl operations.
    // Override: Optional
    // Default Behavior: Creates a DriverChannel instance
    errno_t (*open)(void* _Nonnull _Locked self, unsigned int mode, intptr_t arg, IOChannelRef _Nullable * _Nonnull pOutChannel);

    // Invoked as the result of calling Driver_Close().
    // Override: Optional
    // Default Behavior: Does nothing and returns EOK
    errno_t (*close)(void* _Nonnull _Locked self, IOChannelRef _Nonnull pChannel);


    // Invoked as the result of calling Driver_Read(). A driver subclass should
    // override this method to implement support for the read() system call.
    // Override: Optional
    // Default Behavior: Returns EBADF
    errno_t (*read)(void* _Nonnull self, IOChannelRef _Nonnull ioc, void* _Nonnull buf, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead);

    // Invoked as the result of calling Driver_Write(). A driver subclass should
    // override this method to implement support for the write() system call.
    // Override: Optional
    // Default Behavior: Returns EBADF
    errno_t (*write)(void* _Nonnull self, IOChannelRef _Nonnull ioc, const void* _Nonnull buf, ssize_t nBytesToWrite, ssize_t* _Nonnull nOutBytesWritten);

    // Sets the current position of the I/O channel 'ioc' based on 'offset' and
    // 'whence' and returns the previous position.
    // Override: Optional
    // Default Behavior: Returns EPIPE
    errno_t (*seek)(void* _Nonnull self, IOChannelRef _Nonnull ioc, off_t offset, off_t* _Nullable pOutOldPosition, int whence);
    
    // Invoked as the result of calling Handler_Ioctl(). A driver subclass should
    // override this method to implement support for the ioctl() system call.
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

#endif /* Handler_h */
