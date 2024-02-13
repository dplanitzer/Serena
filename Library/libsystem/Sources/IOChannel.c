//
//  IOChannel.c
//  libsystem
//
//  Created by Dietmar Planitzer on 2/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include <System/IOChannel.h>
#include <System/_syscall.h>
#include <System/_varargs.h>


errno_t IOChannel_Read(int fd, void* _Nonnull buffer, size_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
{
    return (errno_t)_syscall(SC_read, fd, buffer, nBytesToRead, nOutBytesRead);
}

errno_t IOChannel_Write(int fd, const void* _Nonnull buffer, size_t nBytesToWrite, ssize_t* _Nonnull nOutBytesWritten)
{
    return (errno_t)_syscall(SC_write, fd, buffer, nBytesToWrite, nOutBytesWritten);
}

errno_t IOChannel_Close(int fd)
{
    return (errno_t)_syscall(SC_close, fd);
}


IOChannelType IOChannel_GetType(int fd)
{
    long type;

    (void) IOChannel_Control(fd, kIOChannelCommand_GetType, &type);
    return type;
}

int IOChannel_GetMode(int fd)
{
    long mode;

    const errno_t err = IOChannel_Control(fd, kIOChannelCommand_GetMode, &mode);
    return (err == 0) ? mode : 0;
}

errno_t IOChannel_Control(int fd, int cmd, ...)
{
    errno_t err;
    va_list ap;

    va_start(ap, cmd);
    err = (errno_t)_syscall(SC_ioctl, fd, cmd, ap);
    va_end(ap);

    return err;
}
