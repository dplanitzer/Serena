//
//  ProcChannel.c
//  kernel
//
//  Created by Dietmar Planitzer on 5/11/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "ProcChannel.h"
#include "Process.h"
#include "ProcessManager.h"
#include <kpi/fcntl.h>


errno_t ProcChannel_Create(Class* _Nonnull pClass, IOChannelOptions options, int channelType, unsigned int mode, pid_t targetPid, IOChannelRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    ProcChannelRef self = NULL;

    if ((err = IOChannel_Create(pClass, options, channelType, mode, (IOChannelRef*)&self)) == EOK) {
        self->target_pid = targetPid;
    }

    *pOutSelf = (IOChannelRef)self;
    return err;
}

errno_t ProcChannel_ioctl(ProcChannelRef _Nonnull _Locked self, int cmd, va_list ap)
{
    decl_try_err();
    ProcessRef p = ProcessManager_CopyProcessForPid(gProcessManager, self->target_pid);

    if (p) {
        err = Process_vIoctl(p, (IOChannelRef)self, cmd, ap);
        Object_Release(p);
    }
    else {
        err = ESRCH;
    }

    return err;
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
override_func_def(ioctl, ProcChannel, IOChannel)
override_func_def(read, ProcChannel, IOChannel)
override_func_def(write, ProcChannel, IOChannel)
);
