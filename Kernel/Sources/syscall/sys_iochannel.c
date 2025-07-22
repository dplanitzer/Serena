//
//  sys_iochannel.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/6/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "syscalldecls.h"


SYSCALL_1(close, int fd)
{
    ProcessRef pp = vp->proc;

    return IOChannelTable_ReleaseChannel(&pp->ioChannelTable, pa->fd);
}

SYSCALL_4(read, int fd, void* _Nonnull buffer, size_t nBytesToRead, ssize_t* _Nonnull nBytesRead)
{
    decl_try_err();
    ProcessRef pp = vp->proc;
    IOChannelRef pChannel;

    if ((err = IOChannelTable_AcquireChannel(&pp->ioChannelTable, pa->fd, &pChannel)) == EOK) {
        err = IOChannel_Read(pChannel, pa->buffer, __SSizeByClampingSize(pa->nBytesToRead), pa->nBytesRead);
        IOChannelTable_RelinquishChannel(&pp->ioChannelTable, pChannel);
    }
    return err;
}

SYSCALL_4(write, int fd, const void* _Nonnull buffer, size_t nBytesToWrite, ssize_t* _Nonnull nBytesWritten)
{
    decl_try_err();
    ProcessRef pp = vp->proc;
    IOChannelRef pChannel;

    if ((err = IOChannelTable_AcquireChannel(&pp->ioChannelTable, pa->fd, &pChannel)) == EOK) {
        err = IOChannel_Write(pChannel, pa->buffer, __SSizeByClampingSize(pa->nBytesToWrite), pa->nBytesWritten);
        IOChannelTable_RelinquishChannel(&pp->ioChannelTable, pChannel);
    }
    return err;
}

SYSCALL_4(seek, int fd, off_t offset, off_t* _Nullable pOutOldPosition, int whence)
{
    decl_try_err();
    ProcessRef pp = vp->proc;
    IOChannelRef pChannel;

    if ((err = IOChannelTable_AcquireChannel(&pp->ioChannelTable, pa->fd, &pChannel)) == EOK) {
        err = IOChannel_Seek(pChannel, pa->offset, pa->pOutOldPosition, pa->whence);
        IOChannelTable_RelinquishChannel(&pp->ioChannelTable, pChannel);
    }
    return err;
}

SYSCALL_4(fcntl, int fd, int cmd, int* _Nonnull pResult, va_list _Nullable ap)
{
    decl_try_err();
    ProcessRef pp = vp->proc;
    IOChannelRef pChannel;

    *(pa->pResult) = -1;
    if ((err = IOChannelTable_AcquireChannel(&pp->ioChannelTable, pa->fd, &pChannel)) == EOK) {
        err = IOChannel_vFcntl(pChannel, pa->cmd, pa->pResult, pa->ap);
        IOChannelTable_RelinquishChannel(&pp->ioChannelTable, pChannel);
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
        IOChannelTable_RelinquishChannel(&pp->ioChannelTable, pChannel);
    }
    return err;
}
