//
//  apollo.c
//  libsystem
//
//  Created by Dietmar Planitzer on 10/12/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <apollo/apollo.h>
#include <apollo/syscall.h>
#include <abi/_varargs.h>


errno_t creat(const char* path, int options, int permissions, int* fd)
{
    return (errno_t)_syscall(SC_mkfile, path, options, permissions, fd);
}

errno_t open(const char *path, int options, int* fd)
{
    return (errno_t)_syscall(SC_open, path, options, fd);
}

errno_t opendir(const char* path, int* fd)
{
    return (errno_t)_syscall(SC_opendir, path, fd);
}

ssize_t read(int fd, void *buffer, size_t nbytes)
{
    return (ssize_t)_syscall(SC_read, fd, buffer, nbytes);
}

ssize_t write(int fd, const void *buffer, size_t nbytes)
{
    return (ssize_t)_syscall(SC_write, fd, buffer, nbytes);
}

errno_t tell(int fd, off_t* pos)
{
    return (errno_t)_syscall(SC_seek, fd, (off_t)0, pos, (int)S_WHENCE_CUR);
}

errno_t seek(int fd, off_t offset, off_t *oldpos, int whence)
{
    return (errno_t)_syscall(SC_seek, fd, offset, oldpos, whence);
}

errno_t close(int fd)
{
    return (errno_t)_syscall(SC_close, fd);
}


errno_t getcwd(char* buffer, size_t bufferSize)
{
    return (errno_t)_syscall(SC_getcwd, buffer, bufferSize);
}

errno_t setcwd(const char* path)
{
    return (errno_t)_syscall(SC_setcwd, path);
}


errno_t getfileinfo(const char* path, struct _file_info_t* info)
{
    return (errno_t)_syscall(SC_getfileinfo, path, info);
}

errno_t setfileinfo(const char* path, struct _mutable_file_info_t* info)
{
    return (errno_t)_syscall(SC_setfileinfo, path, info);
}

errno_t fgetfileinfo(int fd, struct _file_info_t* info)
{
    return _syscall(SC_fgetfileinfo, fd, info);
}

errno_t fsetfileinfo(int fd, struct _mutable_file_info_t* info)
{
    return (errno_t)_syscall(SC_fsetfileinfo, fd, info);
}


errno_t truncate(const char *path, off_t length)
{
    return (errno_t)_syscall(SC_truncate, path, length);
}

errno_t ftruncate(int fd, off_t length)
{
    return _syscall(SC_ftruncate, fd, length);
}


IOChannelType fgettype(int fd)
{
    long type;

    (void) ioctl(fd, kIOChannelCommand_GetType, &type);
    return type;
}

int fgetmode(int fd)
{
    long mode;

    const errno_t err = ioctl(fd, kIOChannelCommand_GetMode, &mode);
    return (err == 0) ? mode : 0;
}

errno_t ioctl(int fd, int cmd, ...)
{
    errno_t err;
    va_list ap;

    va_start(ap, cmd);
    err = (errno_t)_syscall(SC_ioctl, fd, cmd, ap);
    va_end(ap);

    return err;
}


errno_t access(const char* path, int mode)
{
    return (errno_t)_syscall(SC_access, mode);
}

errno_t unlink(const char* path)
{
    return (errno_t)_syscall(SC_unlink, path);
}

errno_t sys_rename(const char* oldpath, const char* newpath)
{
    return (errno_t)_syscall(SC_rename, oldpath, newpath);
}

errno_t mkdir(const char* path, mode_t mode)
{
    return (errno_t)_syscall(SC_mkdir, path, mode);
}


mode_t getumask(void)
{
    return _syscall(SC_getumask);
}

void setumask(mode_t mask)
{
    _syscall(SC_setumask, mask);
}

pid_t getpid(void)
{
    return _syscall(SC_getpid);
}

pid_t getppid(void)
{
    return _syscall(SC_getppid);
}

uid_t getuid(void)
{
    return _syscall(SC_getuid);
}

errno_t spawnp(const spawn_arguments_t *args, pid_t *rpid)
{
    spawn_arguments_t kargs = *args;

    return (errno_t)_syscall(SC_spawn_process, &kargs, rpid);
}

errno_t waitpid(pid_t pid, waitpid_result_t *result)
{
    return (errno_t)_syscall(SC_waitpid, pid, result);
}

process_arguments_t *getpargs(void)
{
    return (process_arguments_t *) _syscall(SC_getpargs);
}

errno_t nanosleep(const struct timespec *delay)
{
    return (errno_t)_syscall(SC_sleep, delay);
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
