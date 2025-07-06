//
//  Process_Dispatch.c
//  kernel
//
//  Created by Dietmar Planitzer on 3/21/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "ProcessPriv.h"
#include "UWaitQueue.h"


errno_t Process_CreateUWaitQueue(ProcessRef _Nonnull self, int policy, int* _Nullable pOutOd)
{
    decl_try_err();
    UWaitQueueRef p = NULL;

    try(UWaitQueue_Create(policy, &p));
    try(UResourceTable_AdoptResource(&self->uResourcesTable, (UResourceRef) p, pOutOd));
    p = NULL;
    return EOK;

catch:
    UResource_Dispose(p);
    *pOutOd = -1;
    return err;
}

errno_t Process_Wait_UWaitQueue(ProcessRef _Nonnull self, int q, const sigset_t* _Nullable mask)
{
    decl_try_err();
    UWaitQueueRef p;

    if ((err = UResourceTable_AcquireResourceAs(&self->uResourcesTable, q, UWaitQueue, &p)) == EOK) {
        err = UWaitQueue_Wait(p, mask);
        UResourceTable_RelinquishResource(&self->uResourcesTable, p);
    }
    return err;
}

errno_t Process_TimedWait_UWaitQueue(ProcessRef _Nonnull self, int q, const sigset_t* _Nullable mask, int flags, const struct timespec* _Nonnull wtp)
{
    decl_try_err();
    UWaitQueueRef p;

    if ((err = UResourceTable_AcquireResourceAs(&self->uResourcesTable, q, UWaitQueue, &p)) == EOK) {
        err = UWaitQueue_TimedWait(p, mask, flags, wtp);
        UResourceTable_RelinquishResource(&self->uResourcesTable, p);
    }
    return err;
}

errno_t Process_TimedWakeWait_UWaitQueue(ProcessRef _Nonnull self, int q, int oq, const sigset_t* _Nullable mask, int flags, const struct timespec* _Nonnull wtp)
{
    decl_try_err();
    UWaitQueueRef p;
    UWaitQueueRef op;

    if ((err = UResourceTable_AcquireTwoResourcesAs(&self->uResourcesTable, q, UWaitQueue, &p, oq, UWaitQueue, &op)) == EOK) {
        err = UWaitQueue_TimedWakeWait(p, op, mask, flags, wtp);
        UResourceTable_RelinquishTwoResources(&self->uResourcesTable, p, op);
    }
    return err;
}


errno_t Process_Wakeup_UWaitQueue(ProcessRef _Nonnull self, int q, int flags)
{
    decl_try_err();
    UWaitQueueRef p;

    if ((err = UResourceTable_BeginDirectResourceAccessAs(&self->uResourcesTable, q, UWaitQueue, &p)) == EOK) {
        UWaitQueue_Wakeup(p, flags);
        UResourceTable_EndDirectResourceAccess(&self->uResourcesTable);
    }
    return err;
}
