//
//  NullHandler.c
//  kernel
//
//  Created by Dietmar Planitzer on 6/20/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "NullHandler.h"


errno_t NullHandler_Create(void* _Nullable ignore, fd_flags_t flags, HandlerRef _Nullable * _Nonnull pOutHandler)
{
    return Handler_Create(class(NullHandler), FD_TYPE_DRIVER, flags, pOutHandler);
}

errno_t NullHandler_read(struct NullHandler* _Nonnull self, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
{
    if ((Handler_GetFlags(self) & O_RDONLY) == 0) {
        return EBADF;
    }


    // Always return EOF
    *nOutBytesRead = 0;
    return EOK;
}

errno_t NullHandler_write(struct NullHandler* _Nonnull self, const void* _Nonnull pBuffer, ssize_t nBytesToWrite, ssize_t* _Nonnull nOutBytesWritten)
{
    if ((Handler_GetFlags(self) & O_WRONLY ) == 0) {
        return EBADF;
    }


    // Ignore output
    *nOutBytesWritten = nBytesToWrite;
    return EOK;
}

errno_t NullHandler_seek(struct NullHandler* _Nonnull self, off_t offset, off_t* _Nullable pOutNewPos, int whence)
{
    switch (whence) {
        case SEEK_SET:
            if (offset < 0ll) {
                return EINVAL;
            }
            break;

        case SEEK_CUR:
        case SEEK_END:
            break;

        default:
            return EINVAL;
    }

    if (pOutNewPos) {
        *pOutNewPos = offset;
    }
    return EOK;
}


class_func_defs(NullHandler, Handler,
override_func_def(read, NullHandler, Handler)
override_func_def(write, NullHandler, Handler)
override_func_def(seek, NullHandler, Handler)
);
