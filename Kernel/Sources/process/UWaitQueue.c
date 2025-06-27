//
//  UWaitQueue.c
//  kernel
//
//  Created by Dietmar Planitzer on 6/26/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "UWaitQueue.h"
#include <dispatcher/VirtualProcessorScheduler.h>
#include <kern/limits.h>
#include <sys/waitqueue.h>


errno_t UWaitQueue_Create(UWaitQueueRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    UWaitQueueRef self;

    try(UResource_AbstractCreate(&kUWaitQueueClass, (UResourceRef*)&self));
    List_Init(&self->queue);
    *pOutSelf = self;
    return EOK;

catch:
    *pOutSelf = NULL;
    return err;
}

void UWaitQueue_deinit(UWaitQueueRef _Nonnull self)
{
    List_Deinit(&self->queue);
}

errno_t UWaitQueue_Wait(UWaitQueueRef _Nonnull self)
{
    const int sps = VirtualProcessorScheduler_DisablePreemption();
    const int err = VirtualProcessorScheduler_WaitOn(
                            gVirtualProcessorScheduler,
                            &self->queue, 
                            WAIT_INTERRUPTABLE,
                            NULL,
                            NULL);
    VirtualProcessorScheduler_RestorePreemption(sps);
    
    return err;
}

errno_t UWaitQueue_TimedWait(UWaitQueueRef _Nonnull self, int options, const struct timespec* _Nonnull wtp, struct timespec* _Nullable rmtp)
{
    const int sps = VirtualProcessorScheduler_DisablePreemption();
    const int err = VirtualProcessorScheduler_WaitOn(
                            gVirtualProcessorScheduler,
                            &self->queue, 
                            WAIT_INTERRUPTABLE | options,
                            wtp,
                            rmtp);
    VirtualProcessorScheduler_RestorePreemption(sps);
    
    return err;
}

void UWaitQueue_Wakeup(UWaitQueueRef _Nonnull self, int flags)
{
    const int sps = VirtualProcessorScheduler_DisablePreemption();
    
    VirtualProcessorScheduler_WakeUpSome(gVirtualProcessorScheduler,
                                         &self->queue,
                                         ((flags & WAKE_ONE) == WAKE_ONE) ? 1 : INT_MAX,
                                         WAKEUP_REASON_FINISHED,
                                         true);
    VirtualProcessorScheduler_RestorePreemption(sps);
}

class_func_defs(UWaitQueue, UResource,
override_func_def(deinit, UWaitQueue, UResource)
);
