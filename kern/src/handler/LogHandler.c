//
//  LogHandler.c
//  kernel
//
//  Created by Dietmar Planitzer on 6/20/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "LogHandler.h"
#include <kern/log.h>


errno_t LogHandler_Create(InodeRef _Nonnull ip, fd_flags_t flags, HandlerRef _Nullable * _Nonnull pOutHandler)
{
    return PseudoHandler_Create(class(LogHandler), FD_TYPE_DEVICE, ip, flags, pOutHandler);
}

errno_t LogHandler_read(struct LogHandler* _Nonnull self, void* _Nonnull buf, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
{
    if ((Handler_GetFlags(self) & O_RDONLY) == 0) {
        return EBADF;
    }

    *nOutBytesRead = log_read(buf, nBytesToRead);
    return EOK;
}


class_func_defs(LogHandler, PseudoHandler,
override_func_def(read, LogHandler, Handler)
);
