//
//  fcntl.c
//  Apollo
//
//  Created by Dietmar Planitzer on 10/12/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <apollo/apollo.h>
#include <stdlib.h>
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

errno_t spawnp(const spawn_arguments_t *args)
{
    spawn_arguments_t kargs = *args;
    errno_t r;

    // XXX argv and envp may be NULL in user space, user space should pass {path, NULL} if argv == NULL and 'environ' if envp == NULL
//    if (args->argv == NULL) {
//
//    }
    if (kargs.envp == NULL) {
        kargs.envp = environ;
    }

    __failable_syscall(r, SC_spawn_process, &kargs);
    return r;
}

errno_t waitpid(pid_t pid, waitpid_result_t *result)
{
    errno_t r;

    __failable_syscall(r, SC_waitpid, pid, result);
    return r;
}

process_arguments_t *getpargs(void)
{
    return (process_arguments_t *) __syscall(SC_getpargs);
}