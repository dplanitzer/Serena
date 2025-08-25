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

#define _get_pid() \
(const pid_t)IOChannel_GetResource(self)


errno_t ProcChannel_Create(Class* _Nonnull pClass, int channelType, unsigned int mode, pid_t targetPid, IOChannelRef _Nullable * _Nonnull pOutSelf)
{
    return IOChannel_Create(pClass, channelType, mode, (intptr_t)targetPid, pOutSelf);
}

errno_t ProcChannel_ioctl(ProcChannelRef _Nonnull _Locked self, int cmd, va_list ap)
{
    decl_try_err();
    const pid_t pid = _get_pid();
    ProcessRef p = ProcessManager_CopyProcessForPid(gProcessManager, pid);

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
