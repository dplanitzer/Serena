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
        Process_Release(p);
    }
    else {
        err = ESRCH;
    }

    return err;
}


class_func_defs(ProcChannel, IOChannel,
override_func_def(ioctl, ProcChannel, IOChannel)
);
