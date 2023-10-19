//
//  fcntl.c
//  Apollo
//
//  Created by Dietmar Planitzer on 10/12/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <apollo/apollo.h>
#include <syscall.h>


int open(const char *path, int options)
{
    return __syscall(SC_open, path, options);
}

ssize_t read(int fd, void *buffer, size_t nbytes)
{
    ssize_t r;

    __failable_syscall(r, SC_read, fd, buffer, nbytes);
    return r;
}

ssize_t write(int fd, const void *buffer, size_t nbytes)
{
    ssize_t r;

    __failable_syscall(r, SC_write, fd, buffer, nbytes);
    return r;
}

errno_t close(int fd)
{
    errno_t r;

    __failable_syscall(r, SC_close, fd);
    return r;
}


pid_t getpid(void)
{
    return __syscall(SC_getpid);
}

pid_t getppid(void)
{
    return __syscall(SC_getppid);
}
