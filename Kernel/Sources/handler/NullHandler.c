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
#include <sched/mtx.h>


final_class_ivars(NullHandler, Handler,
    mtx_t   mtx;
);


errno_t NullHandler_Create(HandlerRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    struct NullHandler* self;

    err = Handler_Create(class(NullHandler), (HandlerRef*)&self);
    if (err == EOK) {
        mtx_init(&self->mtx);
    }

    *pOutSelf = (HandlerRef)self;
    return err;
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

errno_t NullHandler_seek(struct NullHandler* _Nonnull self, IOChannelRef _Nonnull ioc, off_t offset, off_t* _Nullable pOutNewPos, int whence)
{
    // We use a relatively small max position so that programs that seek to try
    // and get a file size won't receive a huge (eg OFF_MAX) value that would
    // make them run out of memory when they then take this value and do a
    // malloc() with it.
    mtx_lock(&self->mtx);
    const errno_t err = Handler_DoSeek((HandlerRef)self, &ioc->offset, 128ll, offset, whence);
    if (err == EOK && pOutNewPos) {
        *pOutNewPos = ioc->offset;
    }
    mtx_unlock(&self->mtx);

    return err;
}


class_func_defs(NullHandler, Handler,
override_func_def(open, NullHandler, Handler)
override_func_def(read, NullHandler, Handler)
override_func_def(write, NullHandler, Handler)
override_func_def(seek, NullHandler, Handler)
);
