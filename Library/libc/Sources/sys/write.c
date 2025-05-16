//
//  write.c
//  libc
//
//  Created by Dietmar Planitzer on 5/14/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <unistd.h>
#include <sys/_syscall.h>


errno_t write(int fd, const void* _Nonnull buffer, size_t nBytesToWrite, ssize_t* _Nonnull nOutBytesWritten)
{
    return (errno_t)_syscall(SC_write, fd, buffer, nBytesToWrite, nOutBytesWritten);
}
