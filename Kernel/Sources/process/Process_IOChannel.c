//
//  Process_IOChannel.c
//  kernel
//
//  Created by Dietmar Planitzer on 4/10/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "ProcessPriv.h"


errno_t Process_CloseChannel(ProcessRef _Nonnull pProc, int ioc)
{
    decl_try_err();
    IOChannelRef pChannel;

    if ((err = Process_UnregisterIOChannel(pProc, ioc, &pChannel)) == EOK) {
        // The error that close() returns is purely advisory and thus we'll proceed
        // with releasing the resource in any case.
        err = IOChannel_Close(pChannel);
        Object_Release(pChannel);
    }
    return err;
}

errno_t Process_ReadChannel(ProcessRef _Nonnull pProc, int ioc, void* _Nonnull buffer, size_t nBytesToRead, ssize_t* _Nonnull nBytesRead)
{
    decl_try_err();
    IOChannelRef pChannel;

    if ((err = Process_CopyIOChannelForDescriptor(pProc, ioc, &pChannel)) == EOK) {
        err = IOChannel_Read(pChannel, buffer, __SSizeByClampingSize(nBytesToRead), nBytesRead);
        Object_Release(pChannel);
    }
    return err;
}

errno_t Process_WriteChannel(ProcessRef _Nonnull pProc, int ioc, const void* _Nonnull buffer, size_t nBytesToWrite, ssize_t* _Nonnull nBytesWritten)
{
    decl_try_err();
    IOChannelRef pChannel;

    if ((err = Process_CopyIOChannelForDescriptor(pProc, ioc, &pChannel)) == EOK) {
        err = IOChannel_Write(pChannel, buffer, __SSizeByClampingSize(nBytesToWrite), nBytesWritten);
        Object_Release(pChannel);
    }
    return err;
}

errno_t Process_SeekChannel(ProcessRef _Nonnull pProc, int ioc, FileOffset offset, FileOffset* _Nullable pOutOldPosition, int whence)
{
    decl_try_err();
    IOChannelRef pChannel;

    if ((err = Process_CopyIOChannelForDescriptor(pProc, ioc, &pChannel)) == EOK) {
        err = IOChannel_Seek(pChannel, offset, pOutOldPosition, whence);
        Object_Release(pChannel);
    }
    return err;
}

// Sends a I/O Channel or I/O Resource defined command to the I/O Channel or
// resource identified by the given descriptor.
errno_t Process_vIOControl(ProcessRef _Nonnull pProc, int ioc, int cmd, va_list ap)
{
    decl_try_err();
    IOChannelRef pChannel;

    if ((err = Process_CopyIOChannelForDescriptor(pProc, ioc, &pChannel)) == EOK) {
        err = IOChannel_vIOControl(pChannel, cmd, ap);
        Object_Release(pChannel);
    }
    return err;
}
