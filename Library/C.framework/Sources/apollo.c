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

errno_t seek(int fd, off_t offset, off_t *newpos, int whence)
{
    errno_t r;

    __failable_syscall(r, SC_seek, fd, offset, newpos, whence);
    return r;
}

errno_t close(int fd)
{
    errno_t r;

    __failable_syscall(r, SC_close, fd);
    return r;
}


errno_t mkdir(const char* path, mode_t mode)
{
    errno_t r;

    __failable_syscall(r, SC_mkdir, path, mode);
    return r;
}


errno_t getcwd(char* buffer, size_t bufferSize)
{
    errno_t r;

    __failable_syscall(r, SC_getcwd, buffer, bufferSize);
    return r;
}

errno_t setcwd(const char* path)
{
    errno_t r;

    __failable_syscall(r, SC_setcwd, path);
    return r;
}


errno_t getfileinfo(const char* path, struct _file_info_t* info)
{
    errno_t r;

    __failable_syscall(r, SC_getfileinfo, path, info);
    return r;
}

mode_t getumask(void)
{
    return __syscall(SC_getumask);
}

void setumask(mode_t mask)
{
    __syscall(SC_setumask, mask);
}

pid_t getpid(void)
{
    return __syscall(SC_getpid);
}

pid_t getppid(void)
{
    return __syscall(SC_getppid);
}

uid_t getuid(void)
{
    return __syscall(SC_getuid);
}

errno_t spawnp(const spawn_arguments_t *args, pid_t *rpid)
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

    __failable_syscall(r, SC_spawn_process, &kargs, rpid);
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

errno_t nanosleep(const struct timespec *delay)
{
    errno_t r;

    __failable_syscall(r, SC_sleep, delay);
    return r;
}

errno_t usleep(useconds_t delay)
{
    const useconds_t ONE_SECOND = 1000 * 1000;

    if (delay >= ONE_SECOND) {
        return EINVAL;
    }
    if (delay <= 0) {
        return 0;
    }

    struct timespec ts;
    ts.tv_sec = delay / ONE_SECOND;
    ts.tv_nsec = delay - ts.tv_sec * ONE_SECOND;
    return nanosleep(&ts);
}

errno_t sleep(time_t delay)
{
    if (delay <= 0) {
        return 0;
    }

    struct timespec ts;
    ts.tv_sec = delay;
    ts.tv_nsec = 0;
    return nanosleep(&ts);
}
