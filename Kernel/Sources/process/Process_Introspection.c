//
//  Process_Introspection.c
//  kernel
//
//  Created by Dietmar Planitzer on 5/10/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "ProcessPriv.h"
#include "ProcChannel.h"


errno_t Process_Publish(ProcessRef _Locked _Nonnull self)
{
    if (self->catalogId == kCatalogId_None) {
        char buf[12];

        UInt32_ToString(self->pid, 10, false, buf);
        return Catalog_PublishProcess(gProcCatalog, buf, kUserId_Root, kGroupId_Root, FilePermissions_MakeFromOctal(0444), self, &self->catalogId);
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

    //XXX note that Process_OpenFile() is holding the lock
    err = ProcChannel_Create(class(ProcChannel), 0, kIOChannelType_Process, mode, self, pOutChannel);

    return err;
}

errno_t Process_Close(ProcessRef _Nonnull self, IOChannelRef _Nonnull chan)
{
    return EOK;
}

#include <log/Log.h>
errno_t Process_GetInfo(ProcessRef _Nonnull self, procinfo_t* _Nonnull info)
{
    Lock_Lock(&self->lock);
    info->pid = self->pid;
    info->ppid = self->ppid;
    info->virt_size = AddressSpace_GetVirtualSize(self->addressSpace);
    Lock_Unlock(&self->lock);

    return EOK;
}

errno_t Process_GetName(ProcessRef _Nonnull self, char* _Nonnull buf, size_t bufSize)
{
    if (bufSize < 1) {
        return ERANGE;
    }

    Lock_Lock(&self->lock);
    decl_try_err();
    const pargs_t* pa = (const pargs_t*)self->argumentsBase;
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

    Lock_Unlock(&self->lock);
    return err;
}

errno_t Process_vIoctl(ProcessRef _Nonnull self, IOChannelRef _Nonnull pChannel, int cmd, va_list ap)
{
    switch (cmd) {
        case kProcCommand_GetInfo: {
            procinfo_t* info = va_arg(ap, procinfo_t*);

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
