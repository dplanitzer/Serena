//
//  sys_file.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/5/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "syscalldecls.h"
#include <filesystem/IOChannel.h>
#include <process/IOChannelTable.h>
#include <process/ProcessPriv.h>


SYSCALL_1(fd_close, int fd)
{
    ProcessRef pp = vp->proc;

    return IOChannelTable_ReleaseChannel(&pp->ioChannelTable, pa->fd);
}

SYSCALL_4(fd_read, int fd, void* _Nonnull buffer, size_t nBytesToRead, ssize_t* _Nonnull nBytesRead)
{
    decl_try_err();
    ProcessRef pp = vp->proc;
    IOChannelRef pChannel;

    if ((err = IOChannelTable_AcquireChannel(&pp->ioChannelTable, pa->fd, &pChannel)) == EOK) {
        err = IOChannel_Read(pChannel, pa->buffer, __SSizeByClampingSize(pa->nBytesToRead), pa->nBytesRead);
        IOChannel_EndOperation(pChannel);
    }
    return err;
}

SYSCALL_4(fd_write, int fd, const void* _Nonnull buffer, size_t nBytesToWrite, ssize_t* _Nonnull nBytesWritten)
{
    decl_try_err();
    ProcessRef pp = vp->proc;
    IOChannelRef pChannel;

    if ((err = IOChannelTable_AcquireChannel(&pp->ioChannelTable, pa->fd, &pChannel)) == EOK) {
        err = IOChannel_Write(pChannel, pa->buffer, __SSizeByClampingSize(pa->nBytesToWrite), pa->nBytesWritten);
        IOChannel_EndOperation(pChannel);
    }
    return err;
}

SYSCALL_4(fd_seek, int fd, off_t offset, off_t* _Nullable pOutNewPos, int whence)
{
    decl_try_err();
    ProcessRef pp = vp->proc;
    IOChannelRef pChannel;

    if ((err = IOChannelTable_AcquireChannel(&pp->ioChannelTable, pa->fd, &pChannel)) == EOK) {
        err = IOChannel_Seek(pChannel, pa->offset, pa->pOutNewPos, pa->whence);
        IOChannel_EndOperation(pChannel);
    }
    return err;
}

SYSCALL_3(fd_setflags, int fd, int op, int flags)
{
    decl_try_err();
    ProcessRef pp = vp->proc;
    IOChannelRef pChannel;

    if ((err = IOChannelTable_AcquireChannel(&pp->ioChannelTable, pa->fd, &pChannel)) == EOK) {
        err = IOChannel_SetFlags(pChannel, pa->op, pa->flags);
        IOChannel_EndOperation(pChannel);
    }
    return err;
}

SYSCALL_3(ioctl, int fd, int cmd, va_list _Nullable ap)
{
    decl_try_err();
    ProcessRef pp = vp->proc;
    IOChannelRef pChannel;

    if ((err = IOChannelTable_AcquireChannel(&pp->ioChannelTable, pa->fd, &pChannel)) == EOK) {
        err = IOChannel_vIoctl(pChannel, pa->cmd, pa->ap);
        IOChannel_EndOperation(pChannel);
    }
    return err;
}

SYSCALL_2(fd_truncate, int fd, off_t length)
{
    decl_try_err();
    ProcessRef pp = vp->proc;
    IOChannelRef pChannel;

    if ((err = IOChannelTable_AcquireChannel(&pp->ioChannelTable, pa->fd, &pChannel)) == EOK) {
        err = IOChannel_Truncate(pChannel, pa->length);
        IOChannel_EndOperation(pChannel);
    }
    return err;
}

SYSCALL_2(fd_attr, int fd, fs_attr_t* _Nonnull attr)
{
    decl_try_err();
    ProcessRef pp = vp->proc;
    IOChannelRef pChannel;

    if ((err = IOChannelTable_AcquireChannel(&pp->ioChannelTable, pa->fd, &pChannel)) == EOK) {
        err = IOChannel_GetAttributes(pChannel, pa->attr);
        IOChannel_EndOperation(pChannel);
    }
    return err;
}

SYSCALL_3(fd_info, int fd, int flavor, fd_info_ref _Nonnull info)
{
    decl_try_err();
    ProcessRef pp = vp->proc;
    IOChannelRef pChannel;

    if ((err = IOChannelTable_AcquireChannel(&pp->ioChannelTable, pa->fd, &pChannel)) == EOK) {
        err = IOChannel_GetInfo(pChannel, pa->flavor, pa->info);
        IOChannel_EndOperation(pChannel);
    }
    return err;
}

SYSCALL_3(fd_dup, int fd, int min_fd, int* _Nonnull new_fd)
{
    ProcessRef pp = vp->proc;

    return IOChannelTable_DupChannel(&pp->ioChannelTable, pa->fd, pa->min_fd, pa->new_fd);
}

SYSCALL_3(fd_dup_to, int fd, int target_fd, int* _Nonnull new_fd)
{
    ProcessRef pp = vp->proc;

    return IOChannelTable_DupChannelTo(&pp->ioChannelTable, pa->fd, &pp->ioChannelTable, pa->target_fd);
}
