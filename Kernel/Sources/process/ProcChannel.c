//
//  ProcChannel.c
//  kernel
//
//  Created by Dietmar Planitzer on 5/11/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "ProcChannel.h"
#include "Process.h"
#include <kpi/fcntl.h>


errno_t ProcChannel_Create(Class* _Nonnull pClass, IOChannelOptions options, int channelType, unsigned int mode, ProcessRef _Nonnull proc, IOChannelRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    ProcChannelRef self = NULL;

    if ((err = IOChannel_Create(pClass, options, channelType, mode, (IOChannelRef*)&self)) == EOK) {
        self->proc = Object_RetainAs(proc, Process);
    }

    *pOutSelf = (IOChannelRef)self;
    return err;
}

errno_t ProcChannel_finalize(ProcChannelRef _Nonnull self)
{
    if (self->proc) {
        Process_Close(self->proc, (IOChannelRef)self);
        
        Object_Release(self->proc);
        self->proc = NULL;
    }

    return EOK;
}

errno_t ProcChannel_ioctl(ProcChannelRef _Nonnull _Locked self, int cmd, va_list ap)
{
    return Process_vIoctl(self->proc, (IOChannelRef)self, cmd, ap);
}

errno_t ProcChannel_read(ProcChannelRef _Nonnull _Locked self, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
{
    return EPERM;
}

errno_t ProcChannel_write(ProcChannelRef _Nonnull _Locked self, const void* _Nonnull pBuffer, ssize_t nBytesToWrite, ssize_t* _Nonnull nOutBytesWritten)
{
    return EPERM;
}


class_func_defs(ProcChannel, IOChannel,
override_func_def(finalize, ProcChannel, IOChannel)
override_func_def(ioctl, ProcChannel, IOChannel)
override_func_def(read, ProcChannel, IOChannel)
override_func_def(write, ProcChannel, IOChannel)
);
