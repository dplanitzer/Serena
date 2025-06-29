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


errno_t UWaitQueue_Create(int flags, UWaitQueueRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    UWaitQueueRef self = NULL;

    try(UResource_AbstractCreate(&kUWaitQueueClass, (UResourceRef*)&self));
    WaitQueue_Init(&self->wq);
    self->flags = flags & __UWQ_SIGNALLING;
    self->psigs = 0;

catch:
    *pOutSelf = self;
    return err;
}

void UWaitQueue_deinit(UWaitQueueRef _Nonnull self)
{
    WaitQueue_Deinit(&self->wq);
}

errno_t UWaitQueue_Wait(UWaitQueueRef _Nonnull self, unsigned int* _Nullable pOutSigs)
{
    decl_try_err();
    const bool isSignalling = UWaitQueue_IsSignalling(self);
    const int sps = preempt_disable();

    do {
        if (isSignalling && self->psigs) {
            if (pOutSigs) {
                *pOutSigs = self->psigs;
            }
            self->psigs = 0;
            break;
        }

        // Waiting temporarily reenables preemption
        err = WaitQueue_Wait(&self->wq, WAIT_INTERRUPTABLE);
    } while (isSignalling && err == EOK);

    preempt_restore(sps);

    return err;
}

errno_t UWaitQueue_TimedWait(UWaitQueueRef _Nonnull self, int options, const struct timespec* _Nonnull wtp, unsigned int* _Nullable pOutSigs)
{
    decl_try_err();
    const int waitopts = options & TIMER_ABSTIME;
    const bool isSignalling = UWaitQueue_IsSignalling(self);
    const int sps = preempt_disable();
    
    do {
        if (isSignalling && self->psigs) {
            if (pOutSigs) {
                *pOutSigs = self->psigs;
            }
            self->psigs = 0;
            break;
        }

        // Waiting temporarily reenables preemption
        err = WaitQueue_TimedWait(&self->wq, 
                            WAIT_INTERRUPTABLE | waitopts,
                            wtp,
                            NULL);
    } while (isSignalling && err == EOK);

    preempt_restore(sps);
    
    return err;
}

void UWaitQueue_Wakeup(UWaitQueueRef _Nonnull self, int flags, unsigned int sigs)
{
    if (sigs == 0) {
        return;
    }
    
    const bool isSignalling = UWaitQueue_IsSignalling(self);
    const int wakecount = ((flags & WAKE_ONE) == WAKE_ONE) ? 1 : INT_MAX;
    const int sps = preempt_disable();
    
    if (isSignalling) {
        self->psigs |= sigs;
    }

    WaitQueue_WakeUpSome(&self->wq,
                        wakecount,
                        WAKEUP_REASON_FINISHED,
                        true);
    preempt_restore(sps);
}

class_func_defs(UWaitQueue, UResource,
override_func_def(deinit, UWaitQueue, UResource)
);
