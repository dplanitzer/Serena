//
//  Process_IOChannel.c
//  kernel
//
//  Created by Dietmar Planitzer on 4/10/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "ProcessPriv.h"
#include <kern/limits.h>


errno_t Process_CloseChannel(ProcessRef _Nonnull self, int fd)
{
    return IOChannelTable_ReleaseChannel(&self->ioChannelTable, fd);
}

errno_t Process_ReadChannel(ProcessRef _Nonnull self, int fd, void* _Nonnull buffer, size_t nBytesToRead, ssize_t* _Nonnull nBytesRead)
{
    decl_try_err();
    IOChannelRef pChannel;

    if ((err = IOChannelTable_AcquireChannel(&self->ioChannelTable, fd, &pChannel)) == EOK) {
        err = IOChannel_Read(pChannel, buffer, __SSizeByClampingSize(nBytesToRead), nBytesRead);
        IOChannelTable_RelinquishChannel(&self->ioChannelTable, pChannel);
    }
    return err;
}

errno_t Process_WriteChannel(ProcessRef _Nonnull self, int fd, const void* _Nonnull buffer, size_t nBytesToWrite, ssize_t* _Nonnull nBytesWritten)
{
    decl_try_err();
    IOChannelRef pChannel;

    if ((err = IOChannelTable_AcquireChannel(&self->ioChannelTable, fd, &pChannel)) == EOK) {
        err = IOChannel_Write(pChannel, buffer, __SSizeByClampingSize(nBytesToWrite), nBytesWritten);
        IOChannelTable_RelinquishChannel(&self->ioChannelTable, pChannel);
    }
    return err;
}

errno_t Process_SeekChannel(ProcessRef _Nonnull self, int fd, off_t offset, off_t* _Nullable pOutOldPosition, int whence)
{
    decl_try_err();
    IOChannelRef pChannel;

    if ((err = IOChannelTable_AcquireChannel(&self->ioChannelTable, fd, &pChannel)) == EOK) {
        err = IOChannel_Seek(pChannel, offset, pOutOldPosition, whence);
        IOChannelTable_RelinquishChannel(&self->ioChannelTable, pChannel);
    }
    return err;
}

int Process_Fcntl(ProcessRef _Nonnull self, int fd, int cmd, int* _Nonnull pResult, va_list ap)
{
    decl_try_err();
    IOChannelRef pChannel;

    *pResult = -1;
    if ((err = IOChannelTable_AcquireChannel(&self->ioChannelTable, fd, &pChannel)) == EOK) {
        err = IOChannel_vFcntl(pChannel, cmd, pResult, ap);
        IOChannelTable_RelinquishChannel(&self->ioChannelTable, pChannel);
    }
    return err;
}

errno_t Process_Iocall(ProcessRef _Nonnull self, int fd, int cmd, va_list ap)
{
    decl_try_err();
    IOChannelRef pChannel;

    if ((err = IOChannelTable_AcquireChannel(&self->ioChannelTable, fd, &pChannel)) == EOK) {
        err = IOChannel_vIoctl(pChannel, cmd, ap);
        IOChannelTable_RelinquishChannel(&self->ioChannelTable, pChannel);
    }
    return err;
}
