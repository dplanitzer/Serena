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

errno_t Process_SigWait_UWaitQueue(ProcessRef _Nonnull self, int od, const sigset_t* _Nullable mask, sigset_t* _Nonnull pOutSigs)
{
    decl_try_err();
    UWaitQueueRef p;

    if ((err = UResourceTable_AcquireResourceAs(&self->uResourcesTable, od, UWaitQueue, &p)) == EOK) {
        err = UWaitQueue_SigWait(p, mask, pOutSigs);
        UResourceTable_RelinquishResource(&self->uResourcesTable, p);
    }
    return err;
}

errno_t Process_TimedWait_UWaitQueue(ProcessRef _Nonnull self, int od, int flags, const struct timespec* _Nonnull wtp)
{
    decl_try_err();
    UWaitQueueRef p;

    if ((err = UResourceTable_AcquireResourceAs(&self->uResourcesTable, od, UWaitQueue, &p)) == EOK) {
        err = UWaitQueue_TimedWait(p, flags, wtp);
        UResourceTable_RelinquishResource(&self->uResourcesTable, p);
    }
    return err;
}

errno_t Process_SigTimedWait_UWaitQueue(ProcessRef _Nonnull self, int od, const sigset_t* _Nullable mask, sigset_t* _Nonnull pOutSigs, int flags, const struct timespec* _Nonnull wtp)
{
    decl_try_err();
    UWaitQueueRef p;

    if ((err = UResourceTable_AcquireResourceAs(&self->uResourcesTable, od, UWaitQueue, &p)) == EOK) {
        err = UWaitQueue_SigTimedWait(p, mask, pOutSigs, flags, wtp);
        UResourceTable_RelinquishResource(&self->uResourcesTable, p);
    }
    return err;
}


errno_t Process_Wakeup_UWaitQueue(ProcessRef _Nonnull self, int od, int flags, int signo)
{
    decl_try_err();
    UWaitQueueRef p;

    if ((err = UResourceTable_BeginDirectResourceAccessAs(&self->uResourcesTable, od, UWaitQueue, &p)) == EOK) {
        err = UWaitQueue_Wakeup(p, flags, signo);
        UResourceTable_EndDirectResourceAccess(&self->uResourcesTable);
    }
    return err;
}
