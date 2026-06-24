//
//  LogHandler.c
//  kernel
//
//  Created by Dietmar Planitzer on 6/20/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "LogHandler.h"
#include <driver/pseudo/LogDriver.h>


errno_t LogHandler_Create(InodeRef _Nonnull ip, fd_flags_t flags, HandlerRef _Nullable * _Nonnull pOutHandler)
{
    return IODriverHandler_Create(class(LogHandler), FD_TYPE_DEVICE, ip, flags, pOutHandler);
}

errno_t LogHandler_read(struct LogHandler* _Nonnull self, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
{
    const fd_flags_t flags = Handler_GetFlags(self);
    LogDriverRef drv = IODriverHandler_GetDriver(self);

    if ((flags & O_RDONLY) != 0) {
        return Driver_Read(drv, flags, NULL, pBuffer, nBytesToRead, nOutBytesRead);
    }
    else {
        return EBADF;
    }
}

errno_t LogHandler_write(struct LogHandler* _Nonnull self, const void* _Nonnull pBuffer, ssize_t nBytesToWrite, ssize_t* _Nonnull nOutBytesWritten)
{
    const fd_flags_t flags = Handler_GetFlags(self);
    LogDriverRef drv = IODriverHandler_GetDriver(self);

    if ((flags & O_WRONLY) != 0) {
        return Driver_Write(drv, flags, NULL, pBuffer, nBytesToWrite, nOutBytesWritten);
    }
    else {
        return EBADF;
    }
}


class_func_defs(LogHandler, IODriverHandler,
override_func_def(read, LogHandler, Handler)
override_func_def(write, LogHandler, Handler)
);
