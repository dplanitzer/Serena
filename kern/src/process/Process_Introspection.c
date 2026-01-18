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
#include <kpi/fcntl.h>


errno_t Process_Open(ProcessRef _Nonnull self, unsigned int mode, intptr_t arg, IOChannelRef _Nullable * _Nonnull pOutChannel)
{
    return ProcChannel_Create(class(ProcChannel), SEO_FT_PROCESS, mode, self->pid, pOutChannel);
}

static int _Process_GetExactState(ProcessRef _Nonnull _Locked self)
{
    if (self->state == PROC_STATE_RUNNING) {
        // Process is waiting if all vcpus are waiting
        // Process is suspended if all vcpus are suspended
        deque_for_each(&self->vcpu_queue, deque_node_t, it,
            vcpu_t cvp = vcpu_from_owner_qe(it);

            if (cvp->sched_state == SCHED_STATE_RUNNING) {
                return PROC_STATE_RUNNING;
            }
        )

        return PROC_STATE_SLEEPING;
    }

    return self->state;
}

int Process_GetExactState(ProcessRef _Nonnull self)
{
    mtx_lock(&self->mtx);
    const int state = _Process_GetExactState(self);
    mtx_unlock(&self->mtx);

    return state;
}

int Process_GetInexactState(ProcessRef _Nonnull self)
{
    mtx_lock(&self->mtx);
    const int state = self->state;
    mtx_unlock(&self->mtx);

    return state;
}

errno_t Process_GetInfo(ProcessRef _Nonnull self, proc_info_t* _Nonnull info)
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

errno_t Process_GetName(ProcessRef _Nonnull self, char* _Nonnull buf, size_t bufSize)
{
    if (bufSize < 1) {
        return ERANGE;
    }

    mtx_lock(&self->mtx);
    decl_try_err();
    const pargs_t* pa = (const pargs_t*)self->pargs_base;
    const char* arg0 = pa->argv[0];
    const size_t arg0len = strlen(arg0);

    if (bufSize >= arg0len + 1) {
        memcpy(buf, arg0, arg0len);
        buf[arg0len] = '\0';
    }
    else {
        *buf = '\0';
        return ERANGE;
    }

    mtx_unlock(&self->mtx);
    return err;
}

errno_t Process_vIoctl(ProcessRef _Nonnull self, IOChannelRef _Nonnull pChannel, int cmd, va_list ap)
{
    switch (cmd) {
        case kProcCommand_GetInfo: {
            proc_info_t* info = va_arg(ap, proc_info_t*);

            return Process_GetInfo(self, info);
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
