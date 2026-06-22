//
//  NullHandler.c
//  kernel
//
//  Created by Dietmar Planitzer on 6/20/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "NullHandler.h"
#include <driver/pseudo/NullDriver.h>


errno_t NullHandler_Create(NullDriverRef _Nonnull drv, fd_flags_t flags, HandlerRef _Nullable * _Nonnull pOutHandler)
{
    return IODriverHandler_Create(class(NullHandler), FD_TYPE_DRIVER, flags, (DriverRef)drv, pOutHandler);
}

errno_t NullHandler_read(struct NullHandler* _Nonnull self, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
{
    const fd_flags_t flags = Handler_GetFlags(self);
    NullDriverRef drv = IODriverHandler_GetDriver(self);

    if ((flags & O_RDONLY) != 0) {
        return Driver_Read(drv, flags, NULL, pBuffer, nBytesToRead, nOutBytesRead);
    }
    else {
        return EBADF;
    }
}

errno_t NullHandler_write(struct NullHandler* _Nonnull self, const void* _Nonnull pBuffer, ssize_t nBytesToWrite, ssize_t* _Nonnull nOutBytesWritten)
{
    const fd_flags_t flags = Handler_GetFlags(self);
    NullDriverRef drv = IODriverHandler_GetDriver(self);

    if ((flags & O_WRONLY) != 0) {
        return Driver_Write(drv, flags, NULL, pBuffer, nBytesToWrite, nOutBytesWritten);
    }
    else {
        return EBADF;
    }
}


class_func_defs(NullHandler, IODriverHandler,
override_func_def(read, NullHandler, Handler)
override_func_def(write, NullHandler, Handler)
);
