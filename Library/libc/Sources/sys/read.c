//
//  read.c
//  libc
//
//  Created by Dietmar Planitzer on 5/14/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <unistd.h>
#include <sys/_syscall.h>


errno_t read(int fd, void* _Nonnull buffer, size_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
{
    return (errno_t)_syscall(SC_read, fd, buffer, nBytesToRead, nOutBytesRead);
}
