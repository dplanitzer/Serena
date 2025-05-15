//
//  isatty.c
//  libc
//
//  Created by Dietmar Planitzer on 5/14/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <fcntl.h>
#include <sys/_syscall.h>
#include <System/_varargs.h>

int isatty(int fd)
{
    long type;
    const errno_t err = fiocall(fd, kIOChannelCommand_GetType, &type);

    return (err == 0 && type == kIOChannelType_Terminal) ? 1 : 0;
}
