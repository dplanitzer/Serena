//
//  sys_file.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/5/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "syscalldecls.h"
//#include <limits.h>
//#include <ext/timespec.h>
//#include <filesystem/IOChannel.h>
//#include <kpi/directory.h>
//#include <kpi/uid.h>
#include <process/kio.h>


SYSCALL_3(open, const char* _Nonnull path, int oflags, int* _Nonnull pOutIoc)
{
    return _kopen(vp->proc, pa->path, pa->oflags, pa->pOutIoc);
}

SYSCALL_1(fd_close, int fd)
{
    return _kclose(vp->proc, pa->fd);
}

SYSCALL_4(fd_read, int fd, void* _Nonnull buffer, size_t nBytesToRead, ssize_t* _Nonnull nBytesRead)
{
    return _kread(vp->proc, pa->fd, pa->buffer, pa->nBytesToRead, pa->nBytesRead);
}

SYSCALL_4(fd_write, int fd, const void* _Nonnull buffer, size_t nBytesToWrite, ssize_t* _Nonnull nBytesWritten)
{
    return _kwrite(vp->proc, pa->fd, pa->buffer, pa->nBytesToWrite, pa->nBytesWritten);
}

SYSCALL_4(fd_seek, int fd, off_t offset, off_t* _Nullable pOutNewPos, int whence)
{
    return _kseek(vp->proc, pa->fd, pa->offset, pa->pOutNewPos, pa->whence);
}

SYSCALL_3(fd_setflags, int fd, int op, int flags)
{
    return _ksetflags(vp->proc, pa->fd, pa->op, pa->flags);
}

SYSCALL_3(ioctl, int fd, int cmd, va_list _Nullable ap)
{
    return _kioctl(vp->proc, pa->fd, pa->cmd, pa->ap);
}

SYSCALL_2(fd_truncate, int fd, off_t length)
{
    return _kftruncate(vp->proc, pa->fd, pa->length);
}

SYSCALL_2(fd_attr, int fd, fs_attr_t* _Nonnull attr)
{
    return _kfattr(vp->proc, pa->fd, pa->attr);
}

SYSCALL_3(fd_info, int fd, int flavor, fd_info_ref _Nonnull info)
{
    return _kfinfo(vp->proc, pa->fd, pa->flavor, pa->info);
}
