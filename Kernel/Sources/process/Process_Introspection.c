//
//  Process_Introspection.c
//  kernel
//
//  Created by Dietmar Planitzer on 5/10/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "ProcessPriv.h"
#include "ProcChannel.h"
#include <kern/string.h>
#include <kpi/fcntl.h>


errno_t Process_Publish(ProcessRef _Locked _Nonnull self)
{
    if (self->catalogId == kCatalogId_None) {
        char buf[12];

        UInt32_ToString(self->pid, 10, false, buf);
        return Catalog_PublishProcess(gProcCatalog, buf, kUserId_Root, kGroupId_Root, perm_from_octal(0444), self, &self->catalogId);
    }
    else {
        return EOK;
    }
}

errno_t Process_Unpublish(ProcessRef _Locked _Nonnull self)
{
    if (self->catalogId != kCatalogId_None) {
        Catalog_Unpublish(gProcCatalog, kCatalogId_None, self->catalogId);
        self->catalogId = kCatalogId_None;
    }
    return EOK;
}


errno_t Process_Open(ProcessRef _Nonnull self, unsigned int mode, intptr_t arg, IOChannelRef _Nullable * _Nonnull pOutChannel)
{
    decl_try_err();

    //XXX note that the open() syscall is holding the lock
    err = ProcChannel_Create(class(ProcChannel), 0, SEO_FT_PROCESS, mode, self, pOutChannel);

    return err;
}

errno_t Process_Close(ProcessRef _Nonnull self, IOChannelRef _Nonnull chan)
{
    return EOK;
}


static int _proc_calc_state(ProcessRef _Nonnull _Locked self)
{
    if (self->state >= PROC_LIFECYCLE_ZOMBIFYING) {
        return PROC_STATE_ZOMBIE;
    }


    // Process is waiting if all vcpus are waiting
    // Process is suspended if all vcpus are suspended
    size_t nwaiting = 0, nsuspended = 0;
    List_ForEach(&self->vcpu_queue, ListNode,
        vcpu_t cvp = VP_FROM_OWNER_NODE(pCurNode);

        if (cvp->suspension_count > 0) {
            nsuspended++;
        }
        else if (cvp->sched_state != SCHED_STATE_RUNNING) {
            nwaiting++;
        }
        else {
            break;
        }
    );


    if (self->vcpu_count == nwaiting) {
        return PROC_STATE_WAITING;
    }
    else if (self->vcpu_count == nsuspended) {
        return PROC_STATE_SUSPENDED;
    }
    else {
        return PROC_STATE_RUNNING;
    }
}

errno_t Process_GetInfo(ProcessRef _Nonnull self, proc_info_t* _Nonnull info)
{
    mtx_lock(&self->mtx);
    info->ppid = self->ppid;
    info->pid = self->pid;
    info->pgrp = self->pgrp;
    info->sid = self->sid;
    info->vcpu_count = self->vcpu_count;
    info->state = _proc_calc_state(self);
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
    const size_t arg0len = String_Length(arg0);

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
