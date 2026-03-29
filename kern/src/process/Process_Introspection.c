//
//  Process_Introspection.c
//  kernel
//
//  Created by Dietmar Planitzer on 5/10/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "ProcessPriv.h"
#include "ProcChannel.h"
#include <string.h>
#include <kpi/file.h>


errno_t Process_Open(ProcessRef _Nonnull self, unsigned int mode, intptr_t arg, IOChannelRef _Nullable * _Nonnull pOutChannel)
{
    return ProcChannel_Create(class(ProcChannel), SEO_FT_PROCESS, mode, self->pid, pOutChannel);
}

static int _Process_GetExactState(ProcessRef _Nonnull _Locked self)
{
    if (self->run_state == PROC_STATE_ALIVE) {
        // Process is waiting if all vcpus are waiting
        // Process is suspended if all vcpus are suspended
        deque_for_each(&self->vcpu_queue, deque_node_t, it,
            vcpu_t cvp = vcpu_from_owner_qe(it);

            if (cvp->run_state == VCPU_RUST_RUNNING) {
                return PROC_STATE_RUNNING_OLD;
            }
        )

        return PROC_STATE_SLEEPING_OLD;
    }

    switch (self->run_state) {
        case PROC_STATE_STOPPED:    return PROC_STATE_STOPPED_OLD;
        case PROC_STATE_EXITING:    return PROC_STATE_EXITING_OLD;
        default:     return PROC_STATE_ZOMBIE_OLD;
    }
}

int Process_GetExactState(ProcessRef _Nonnull self)
{
    mtx_lock(&self->mtx);
    const int state = _Process_GetExactState(self);
    mtx_unlock(&self->mtx);

    return state;
}

int Process_GetState(ProcessRef _Nonnull self)
{
    mtx_lock(&self->mtx);
    const int state = self->run_state;
    mtx_unlock(&self->mtx);

    return state;
}

errno_t Process_GetInfo2(ProcessRef _Nonnull self, proc_info_old_t* _Nonnull info)
{
    mtx_lock(&self->mtx);
    info->ppid = self->ppid;
    info->pid = self->pid;
    info->pgrp = self->pgrp;
    info->sid = self->sid;
    info->vcpu_count = self->vcpu_count;
    info->state = _Process_GetExactState(self);
    info->uid = FileManager_GetRealUserId(&self->fm);
    mtx_unlock(&self->mtx);

    info->virt_size = AddressSpace_GetVirtualSize(&self->addr_space);

    return EOK;
}

errno_t Process_vIoctl(ProcessRef _Nonnull self, IOChannelRef _Nonnull pChannel, int cmd, va_list ap)
{
    switch (cmd) {
        case kProcCommand_GetInfo: {
            proc_info_old_t* info = va_arg(ap, proc_info_old_t*);

            return Process_GetInfo2(self, info);
        }

        case kProcCommand_GetName: {
            void* buf = va_arg(ap, void*);
            size_t bufSize = va_arg(ap, size_t);

            return Process_GetName(self, buf, bufSize);
        }

        default:
            return ENOTIOCTLCMD;
    }
}

errno_t Process_Ioctl(ProcessRef _Nonnull self, IOChannelRef _Nonnull pChannel, int cmd, ...)
{
    decl_try_err();

    va_list ap;
    va_start(ap, cmd);
    err = Process_vIoctl(self, pChannel, cmd, ap);
    va_end(ap);

    return err;
}
