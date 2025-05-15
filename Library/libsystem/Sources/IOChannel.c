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



IOChannelType fgettype(int fd)
{
    long type;

    (void) fiocall(fd, kIOChannelCommand_GetType, &type);
    return type;
}

unsigned int fgetmode(int fd)
{
    unsigned int mode;

    const errno_t err = fiocall(fd, kIOChannelCommand_GetMode, &mode);
    return (err == 0) ? mode : 0;
}

errno_t fiocall(int fd, int cmd, ...)
{
    errno_t err;
    va_list ap;

    va_start(ap, cmd);
    err = (errno_t)_syscall(SC_ioctl, fd, cmd, ap);
    va_end(ap);

    return err;
}
