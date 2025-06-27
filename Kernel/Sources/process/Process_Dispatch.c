//
//  Process_Dispatch.c
//  kernel
//
//  Created by Dietmar Planitzer on 3/21/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "ProcessPriv.h"
#include "UWaitQueue.h"


errno_t Process_CreateUWaitQueue(ProcessRef _Nonnull self, int* _Nullable pOutOd)
{
    decl_try_err();
    UWaitQueueRef p = NULL;

    try(UWaitQueue_Create(&p));
    try(UResourceTable_AdoptResource(&self->uResourcesTable, (UResourceRef) p, pOutOd));
    p = NULL;
    return EOK;

catch:
    UResource_Dispose(p);
    *pOutOd = -1;
    return err;
}

errno_t Process_Wait_UWaitQueue(ProcessRef _Nonnull self, int od)
{
    decl_try_err();
    UWaitQueueRef p;

    if ((err = UResourceTable_AcquireResourceAs(&self->uResourcesTable, od, UWaitQueue, &p)) == EOK) {
        err = UWaitQueue_Wait(p);
        UResourceTable_RelinquishResource(&self->uResourcesTable, p);
    }
    return err;
}

errno_t Process_TimedWait_UWaitQueue(ProcessRef _Nonnull self, int od, int options, const struct timespec* _Nonnull wtp, struct timespec* _Nullable rmtp)
{
    decl_try_err();
    UWaitQueueRef p;

    if ((err = UResourceTable_AcquireResourceAs(&self->uResourcesTable, od, UWaitQueue, &p)) == EOK) {
        err = UWaitQueue_TimedWait(p, options, wtp, rmtp);
        UResourceTable_RelinquishResource(&self->uResourcesTable, p);
    }
    return err;
}

errno_t Process_Wakeup_UWaitQueue(ProcessRef _Nonnull self, int od, int options)
{
    decl_try_err();
    UWaitQueueRef p;

    if ((err = UResourceTable_BeginDirectResourceAccessAs(&self->uResourcesTable, od, UWaitQueue, &p)) == EOK) {
        UWaitQueue_Wakeup(p, options);
        UResourceTable_EndDirectResourceAccess(&self->uResourcesTable);
    }
    return err;
}
