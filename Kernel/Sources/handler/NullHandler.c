//
//  NullHandler.c
//  kernel
//
//  Created by Dietmar Planitzer on 8/3/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "NullHandler.h"
#include "HandlerChannel.h"
#include <kpi/fcntl.h>


final_class_ivars(NullHandler, Handler,
);


errno_t NullHandler_Create(HandlerRef _Nullable * _Nonnull pOutSelf)
{
    return Handler_Create(class(NullHandler), pOutSelf);
}

errno_t NullHandler_open(HandlerRef _Nonnull self, unsigned int mode, intptr_t arg, IOChannelRef _Nullable * _Nonnull pOutChannel)
{
    return HandlerChannel_Create(self, SEO_FT_DRIVER, mode, 0, pOutChannel);
}

errno_t NullHandler_read(HandlerRef _Nonnull self, IOChannelRef _Nonnull ioc, void* _Nonnull buf, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
{
    // Always return EOF
    *nOutBytesRead = 0;
    return EOK;
}

errno_t NullHandler_write(HandlerRef _Nonnull self, IOChannelRef _Nonnull ioc, const void* _Nonnull buf, ssize_t nBytesToWrite, ssize_t* _Nonnull nOutBytesWritten)
{
    // Ignore output
    return EOK;
}


class_func_defs(NullHandler, Handler,
override_func_def(open, NullHandler, Handler)
override_func_def(read, NullHandler, Handler)
override_func_def(write, NullHandler, Handler)
);
