//
//  LogHandler.c
//  kernel
//
//  Created by Dietmar Planitzer on 8/3/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "LogHandler.h"
#include "HandlerChannel.h"
#include <kpi/fcntl.h>
#include <log/Log.h>


final_class_ivars(LogHandler, Handler,
);


errno_t LogHandler_Create(HandlerRef _Nullable * _Nonnull pOutSelf)
{
    return Handler_Create(class(LogHandler), 0, (HandlerRef*)pOutSelf);
}

errno_t LogHandler_open(HandlerRef _Nonnull self, unsigned int mode, intptr_t arg, IOChannelRef _Nullable * _Nonnull pOutChannel)
{
    return HandlerChannel_Create(self, SEO_FT_DRIVER, mode, 0, pOutChannel);
}

errno_t LogHandler_read(HandlerRef _Nonnull self, IOChannelRef _Nonnull ioc, void* _Nonnull buf, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
{
    *nOutBytesRead = log_read(buf, nBytesToRead);
    return EOK;
}

errno_t LogHandler_write(HandlerRef _Nonnull self, IOChannelRef _Nonnull ioc, const void* _Nonnull buf, ssize_t nBytesToWrite, ssize_t* _Nonnull nOutBytesWritten)
{
    log_write(buf, nBytesToWrite);
    *nOutBytesWritten = nBytesToWrite;
    return EOK;
}


class_func_defs(LogHandler, Handler,
override_func_def(open, LogHandler, Handler)
override_func_def(read, LogHandler, Handler)
override_func_def(write, LogHandler, Handler)
);
