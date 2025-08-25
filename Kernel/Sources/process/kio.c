//
//  kio.c
//  kernel
//
//  Created by Dietmar Planitzer on 8/24/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "kio.h"
#include <filesystem/IOChannel.h>
#include <process/IOChannelTable.h>
#include <process/ProcessPriv.h>


errno_t _kopen(ProcessRef _Nonnull pp, const char* _Nonnull path, int oflags, int* _Nonnull pOutIoc)
{
    decl_try_err();
    IOChannelRef chan;

    mtx_lock(&pp->mtx);
    err = FileManager_OpenFile(&pp->fm, path, oflags, &chan);
    if (err == EOK) {
        err = IOChannelTable_AdoptChannel(&pp->ioChannelTable, chan, pOutIoc);
    }
    mtx_unlock(&pp->mtx);

    if (err != EOK) {
        IOChannel_Release(chan);
        *pOutIoc = -1;
    }
    return err;
}

errno_t _kclose(ProcessRef _Nonnull pp, int fd)
{
    return IOChannelTable_ReleaseChannel(&pp->ioChannelTable, fd);
}

errno_t _kread(ProcessRef _Nonnull pp, int fd, void* _Nonnull buffer, size_t nBytesToRead, ssize_t* _Nonnull nBytesRead)
{
    decl_try_err();
    IOChannelRef pChannel;

    if ((err = IOChannelTable_AcquireChannel(&pp->ioChannelTable, fd, &pChannel)) == EOK) {
        err = IOChannel_Read(pChannel, buffer, __SSizeByClampingSize(nBytesToRead), nBytesRead);
        IOChannelTable_RelinquishChannel(&pp->ioChannelTable, pChannel);
    }
    return err;
}

errno_t _kwrite(ProcessRef _Nonnull pp, int fd, const void* _Nonnull buffer, size_t nBytesToWrite, ssize_t* _Nonnull nBytesWritten)
{
    decl_try_err();
    IOChannelRef pChannel;

    if ((err = IOChannelTable_AcquireChannel(&pp->ioChannelTable, fd, &pChannel)) == EOK) {
        err = IOChannel_Write(pChannel, buffer, __SSizeByClampingSize(nBytesToWrite), nBytesWritten);
        IOChannelTable_RelinquishChannel(&pp->ioChannelTable, pChannel);
    }
    return err;
}

errno_t _kseek(ProcessRef _Nonnull pp, int fd, off_t offset, off_t* _Nullable pOutNewPos, int whence)
{
    decl_try_err();
    IOChannelRef pChannel;

    if ((err = IOChannelTable_AcquireChannel(&pp->ioChannelTable, fd, &pChannel)) == EOK) {
        err = IOChannel_Seek(pChannel, offset, pOutNewPos, whence);
        IOChannelTable_RelinquishChannel(&pp->ioChannelTable, pChannel);
    }
    return err;
}

errno_t _kfcntl(ProcessRef _Nonnull pp, int fd, int cmd, int* _Nonnull pResult, va_list _Nullable ap)
{
    decl_try_err();
    IOChannelRef pChannel;

    *pResult = -1;
    if ((err = IOChannelTable_AcquireChannel(&pp->ioChannelTable, fd, &pChannel)) == EOK) {
        err = IOChannel_vFcntl(pChannel, cmd, pResult, ap);
        IOChannelTable_RelinquishChannel(&pp->ioChannelTable, pChannel);
    }
    return err;
}

errno_t _kioctl(ProcessRef _Nonnull pp, int fd, int cmd, va_list _Nullable ap)
{
    decl_try_err();
    IOChannelRef pChannel;

    if ((err = IOChannelTable_AcquireChannel(&pp->ioChannelTable, fd, &pChannel)) == EOK) {
        err = IOChannel_vIoctl(pChannel, cmd, ap);
        IOChannelTable_RelinquishChannel(&pp->ioChannelTable, pChannel);
    }
    return err;
}


errno_t _kfstat(ProcessRef _Nonnull pp, int fd, struct stat* _Nonnull pOutInfo)
{
    decl_try_err();
    IOChannelRef pChannel;

    if ((err = IOChannelTable_AcquireChannel(&pp->ioChannelTable, fd, &pChannel)) == EOK) {
        err = IOChannel_GetFileInfo(pChannel, pOutInfo);
        IOChannelTable_RelinquishChannel(&pp->ioChannelTable, pChannel);
    }
    return err;
}

errno_t _kftruncate(ProcessRef _Nonnull pp, int fd, off_t length)
{
    decl_try_err();
    IOChannelRef pChannel;

    if ((err = IOChannelTable_AcquireChannel(&pp->ioChannelTable, fd, &pChannel)) == EOK) {
        err = IOChannel_Truncate(pChannel, length);
        IOChannelTable_RelinquishChannel(&pp->ioChannelTable, pChannel);
    }
    return err;
}
